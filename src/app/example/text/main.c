/*
 * example/text/main.c - gfx sandbox: SDF text rendering.
 *
 * Renders one string at the top-left of the window through the font layer and
 * instancing, at TEXT_SCALE times the rasterised glyph size (1.0 = one atlas
 * texel per screen pixel):
 * - font/font_atlas: Roboto is parsed and its printable-ASCII glyphs are
 *   rasterised as SDFs into one R8 atlas, uploaded once; the build arena
 *   (ttf bytes, atlas pixels, packer) is then freed and only the glyph table
 *   survives
 * - instancing: a single unit quad drawn once per visible glyph; each
 *   instance carries the quad's screen-pixel position (laid out at
 *   TEXT_SCALE), its atlas UV origin, and its TEXEL size - the vertex shader
 *   scales the corner expansion by u_scale, so the UV math never changes
 * - projection: rc_mat44f_make_ortho_2d, so vertex positions are plain
 *   pixels with the origin at the window's top-left, y down
 * - SDF shader: the fragment shader recovers a signed distance in atlas
 *   texels from the sampled value (needs rc_font.spread), converts it to
 *   screen pixels via the UV derivatives, and applies a one-pixel
 *   antialiasing ramp - exact at any scale or rotation, and free of the
 *   noise that fwidth of the raw sampled value picks up near corners
 * - atlas view: the whole atlas texture is drawn at 1:1 below the string
 *   (raw SDF values on a dark blue quad) so the packing is visible
 *
 * Run from this binary's own directory so data/fonts/Roboto-Regular.ttf
 * resolves (it is staged there by the build).
 */

#include <stddef.h>
#include <string.h>

#include "richc/app/app.h"
#include "richc/file.h"
#include "richc/font/font_atlas.h"
#include "richc/gfx/bindings.h"
#include "richc/gfx/buffer.h"
#include "richc/gfx/encoder.h"
#include "richc/gfx/gfx.h"
#include "richc/gfx/pass.h"
#include "richc/gfx/pipeline.h"
#include "richc/gfx/shader.h"
#include "richc/gfx/texture.h"
#include "richc/math/mat44f.h"

#define PIXEL_SIZE  18.0f   /* em height, px, as rasterised into the atlas */
#define TEXT_SCALE  2.0f    /* on-screen size relative to the rasterised size */
#define MARGIN      16.0f   /* distance from the window's top-left corner, px */
#define ATLAS_SIZE  256     /* fits 18 px ASCII; 36 px needs 512 */

static const char message[] = "The quick brown fox jumps over the lazy dog.";

/* ---- data ---- */

/* One instance per visible glyph; the quad corners expand these in the VS. */
typedef struct glyph_instance {
    rc_vec2f pos;      /* quad top-left, screen pixels                    */
    rc_vec2f uv_pos;   /* quad top-left, normalized atlas UV              */
    rc_vec2f size;     /* quad size, pixels (== atlas texels at 100%)     */
} glyph_instance;

typedef struct frame_uniforms {
    rc_mat44f proj;
    rc_vec2f  atlas_dim;   /* atlas size, texels */
    float     spread;      /* SDF half-range, texels (rc_font.spread) */
    float     scale;       /* screen pixels per atlas texel */
} frame_uniforms;

/* A unit quad as a triangle strip; instances scale and place it. */
static const rc_vec2f quad_corners[4] = {
    {0.0f, 0.0f},
    {1.0f, 0.0f},
    {0.0f, 1.0f},
    {1.0f, 1.0f},
};

static const char vs_src[] =
    "layout(location = 0) in vec2 a_corner;\n"
    "layout(location = 1) in vec2 i_pos;\n"
    "layout(location = 2) in vec2 i_uv_pos;\n"
    "layout(location = 3) in vec2 i_size;\n"
    "out vec2 v_uv;\n"
    "layout(std140) uniform FrameUniforms {\n"
    "    mat4 u_proj;\n"
    "    vec2 u_atlas_dim;\n"
    "    float u_spread;\n"
    "    float u_scale;\n"
    "};\n"
    "void main() {\n"
    "    v_uv = i_uv_pos + a_corner * (i_size / u_atlas_dim);\n"
    "    vec2 pos = i_pos + a_corner * i_size * u_scale;\n"
    "    gl_Position = rc_clip(u_proj * vec4(pos, 0.0, 1.0));\n"
    "}\n";

/*
 * SDF coverage via UV derivatives.  The atlas byte maps [-spread, +spread]
 * texels of signed distance onto [0, 1] around 0.5, so the sampled value
 * converts back to a distance in texels exactly.  The derivative of the
 * texel coordinate per screen pixel then rescales that to a distance in
 * screen pixels, and a one-pixel ramp around the outline antialiases it.
 */
static const char fs_src[] =
    "uniform sampler2D u_atlas;\n"
    "in vec2 v_uv;\n"
    "layout(std140) uniform FrameUniforms {\n"
    "    mat4 u_proj;\n"
    "    vec2 u_atlas_dim;\n"
    "    float u_spread;\n"
    "    float u_scale;\n"
    "};\n"
    "out vec4 o_color;\n"
    "void main() {\n"
    "    float dist_texels = (texture(u_atlas, v_uv).r - 0.5) * 2.0 * u_spread;\n"
    "    vec2 st = v_uv * u_atlas_dim;\n"
    "    float texels_per_px = length(vec2(length(dFdx(st)), length(dFdy(st)))) * 0.70710678;\n"
    "    float dist_px = dist_texels / max(texels_per_px, 1e-6);\n"
    "    float alpha = clamp(dist_px + 0.5, 0.0, 1.0);\n"
    "    o_color = vec4(vec3(alpha), alpha);   // premultiplied white\n"
    "}\n";

/*
 * Atlas view: the raw SDF bytes on a dark blue quad.  Value 0 (far outside,
 * the unpacked gaps) shows as dark blue, so both the packed glyph cells and
 * the atlas extent read clearly against the near-black window clear.
 */
static const char atlas_fs_src[] =
    "uniform sampler2D u_atlas;\n"
    "in vec2 v_uv;\n"
    "out vec4 o_color;\n"
    "void main() {\n"
    "    float v = texture(u_atlas, v_uv).r;\n"
    "    o_color = vec4(mix(vec3(0.01, 0.025, 0.11), vec3(1.0), v), 1.0);\n"
    "}\n";

/* ---- state ---- */

static struct {
    rc_arena             arena;         /* persistent (gfx, glyph table) */
    rc_arena             frame_arena;   /* rewound every frame */

    rc_gfx_buffer        quad_vbuf;
    rc_gfx_buffer        instance_buf;
    uint32_t             instance_count;
    rc_gfx_buffer        atlas_quad_buf;   /* one instance: the whole atlas at 1:1 */
    rc_gfx_texture       atlas_tex;
    rc_vec2f             atlas_dim;
    float                spread;
    rc_gfx_sampler       sampler;
    rc_gfx_shader        shader;
    rc_gfx_shader        atlas_shader;
    rc_gfx_simple_layout layout;
    rc_gfx_bind_group    group0;
    rc_gfx_pipeline      pip;
    rc_gfx_pipeline      atlas_pip;
} state;

/* ---- setup ---- */

/*
 * Build the SDF atlas and the per-glyph instance list.  Everything transient
 * (ttf bytes, atlas pixels, packer) lives in a local build arena freed on
 * return; the glyph table would persist in state.arena, though this example
 * lays out its one static string right here and keeps only the instances.
 */
static void text_setup(void)
{
    rc_arena build_arena = rc_arena_make_default();

    rc_file_load_binary_result ttf =
        rc_file_load_binary(RC_STR("data/fonts/Roboto-Regular.ttf"), 0, &build_arena);
    RC_PANIC(ttf.error == RC_FILE_OK);

    rc_font_result font = rc_font_make(ttf.contents.view, PIXEL_SIZE, &build_arena);
    RC_PANIC(font.error == RC_FONT_OK);

    rc_glyph_table table = rc_glyph_table_make(&state.arena);
    rc_font_atlas atlas = rc_font_atlas_make(font.font,
        rc_vec2i_make(ATLAS_SIZE, ATLAS_SIZE), 1, &build_arena);
    uint32_t missed = rc_font_atlas_add_range(&atlas, &table,
        RC_GLYPH_ASCII_FIRST, RC_GLYPH_ASCII_LAST, &build_arena, &state.arena);
    RC_PANIC(missed == 0);

    state.atlas_tex = rc_gfx_texture_from_image(atlas.image, false, false);
    state.atlas_dim = rc_vec2f_make((float)atlas.image.size.x, (float)atlas.image.size.y);
    state.spread = font.font.spread;

    // lay the string out along the baseline in SCALED screen space: metrics
    // (ascent, offset, advance) are in rasterised-glyph pixels, so multiply
    // each by TEXT_SCALE; instance sizes stay in texels (the vertex shader
    // applies u_scale to the quad expansion, leaving the UV math untouched)
    glyph_instance instances[sizeof(message) - 1];
    uint32_t count = 0;
    rc_vec2f pen = rc_vec2f_make(MARGIN, MARGIN + font.font.ascent * TEXT_SCALE);
    for (uint32_t i = 0; i < sizeof(message) - 1; i += 1) {
        rc_glyph glyph = rc_glyph_table_find(&table, (uint32_t)message[i]);
        rc_vec2f uv_size = rc_box2f_size(glyph.uv);
        if (uv_size.x > 0.0f) {   // whitespace has a zero-size quad
            instances[count] = (glyph_instance) {
                .pos = rc_vec2f_add(pen, rc_vec2f_scalar_mul(glyph.offset, TEXT_SCALE)),
                .uv_pos = rc_box2f_min(glyph.uv),
                .size = rc_vec2f_component_mul(uv_size, state.atlas_dim),
            };
            count += 1;
        }
        pen.x += glyph.advance * TEXT_SCALE;
    }
    state.instance_count = count;

    state.instance_buf = rc_gfx_buffer_make(&(rc_gfx_buffer_desc) {
        .size = count * (uint32_t)sizeof(glyph_instance),
        .usage = RC_GFX_BUFFER_USAGE_VERTEX,
        .data = {
            .data = (const uint8_t *)instances,
            .num = count * (uint32_t)sizeof(glyph_instance),
        },
        .label = RC_STR("glyph instances"),
    });

    // one more instance through the same vertex path: the whole atlas at 1:1
    // (uv [0,1] over a quad of atlas_dim pixels; its draw passes u_scale = 1),
    // a margin below the text line
    glyph_instance atlas_quad = {
        .pos = rc_vec2f_make(MARGIN,
                             MARGIN + (font.font.ascent - font.font.descent) * TEXT_SCALE + MARGIN),
        .uv_pos = rc_vec2f_make(0.0f, 0.0f),
        .size = state.atlas_dim,
    };
    state.atlas_quad_buf = rc_gfx_buffer_make(&(rc_gfx_buffer_desc) {
        .size = sizeof(atlas_quad),
        .usage = RC_GFX_BUFFER_USAGE_VERTEX,
        .data = {
            .data = (const uint8_t *)&atlas_quad,
            .num = sizeof(atlas_quad),
        },
        .label = RC_STR("atlas view instance"),
    });

    rc_arena_deinit(&build_arena);
}

static void gfx_setup(void)
{
    rc_gfx_init(&(rc_gfx_desc) {
        .arena = &state.arena,
    });

    text_setup();

    state.quad_vbuf = rc_gfx_buffer_make(&(rc_gfx_buffer_desc) {
        .size = sizeof(quad_corners),
        .usage = RC_GFX_BUFFER_USAGE_VERTEX,
        .data = {
            .data = (const uint8_t *)quad_corners,
            .num = sizeof(quad_corners),
        },
        .label = RC_STR("glyph quad"),
    });

    // bilinear filtering is essential: the SDF's subpixel edge positions live
    // between texels.  No mip filtering - the atlas has a single level.
    state.sampler = rc_gfx_sampler_make(&(rc_gfx_sampler_desc) {
        .min_filter = RC_GFX_FILTER_LINEAR,
        .mag_filter = RC_GFX_FILTER_LINEAR,
        .label = RC_STR("atlas sampler"),
    });

    state.shader = rc_gfx_shader_make(&(rc_gfx_shader_desc) {
        .vs_source = RC_STR(vs_src),
        .fs_source = RC_STR(fs_src),
        .uniform_blocks = (const rc_gfx_shader_uniform_block[]) {
            {
                .glsl_name = RC_STR("FrameUniforms"),
                .group = 0,
                .binding = 0,
                .size = sizeof(frame_uniforms),
                .members = (const rc_gfx_uniform_member[]) {
                    {.name = RC_STR("u_proj"), .offset = offsetof(frame_uniforms, proj), .size = sizeof(rc_mat44f)},
                    {.name = RC_STR("u_atlas_dim"), .offset = offsetof(frame_uniforms, atlas_dim), .size = sizeof(rc_vec2f)},
                    {.name = RC_STR("u_spread"), .offset = offsetof(frame_uniforms, spread), .size = sizeof(float)},
                    {.name = RC_STR("u_scale"), .offset = offsetof(frame_uniforms, scale), .size = sizeof(float)},
                },
                .member_count = 4,
            },
        },
        .uniform_block_count = 1,
        .texture_samplers = (const rc_gfx_shader_texture_sampler_pair[]) {
            {
                .glsl_name = RC_STR("u_atlas"),
                .texture_binding = 1,
                .sampler_binding = 2,
            },
        },
        .texture_sampler_count = 1,
        .label = RC_STR("text shader"),
    });

    // same vertex shader and bindings; only the fragment stage differs
    state.atlas_shader = rc_gfx_shader_make(&(rc_gfx_shader_desc) {
        .vs_source = RC_STR(vs_src),
        .fs_source = RC_STR(atlas_fs_src),
        .uniform_blocks = (const rc_gfx_shader_uniform_block[]) {
            {
                .glsl_name = RC_STR("FrameUniforms"),
                .group = 0,
                .binding = 0,
                .size = sizeof(frame_uniforms),
            },
        },
        .uniform_block_count = 1,
        .texture_samplers = (const rc_gfx_shader_texture_sampler_pair[]) {
            {
                .glsl_name = RC_STR("u_atlas"),
                .texture_binding = 1,
                .sampler_binding = 2,
            },
        },
        .texture_sampler_count = 1,
        .label = RC_STR("atlas view shader"),
    });

    state.layout = rc_gfx_simple_layout_make(
        (const rc_gfx_bind_group_layout_entry[]) {
            {
                .binding = 0,
                .visibility = RC_GFX_STAGE_VERTEX | RC_GFX_STAGE_FRAGMENT,
                .type = RC_GFX_BINDING_UNIFORM_BUFFER,
                .has_dynamic_offset = true,
                .min_binding_size = sizeof(frame_uniforms),
            },
            {
                .binding = 1,
                .visibility = RC_GFX_STAGE_FRAGMENT,
                .type = RC_GFX_BINDING_TEXTURE,
            },
            {
                .binding = 2,
                .visibility = RC_GFX_STAGE_FRAGMENT,
                .type = RC_GFX_BINDING_SAMPLER,
            },
        },
        3,
        RC_STR("text layout"));

    state.group0 = rc_gfx_bind_group_make(&(rc_gfx_bind_group_desc) {
        .layout = state.layout.group0,
        .entries = {
            {
                .binding = 0,
                .buffer = rc_gfx_uniform_buffer(),
                .buffer_size = sizeof(frame_uniforms),
            },
            {
                .binding = 1,
                .texture = state.atlas_tex,
            },
            {
                .binding = 2,
                .sampler = state.sampler,
            },
        },
        .entry_count = 3,
        .label = RC_STR("text bind group"),
    });

    state.pip = rc_gfx_pipeline_make(&(rc_gfx_pipeline_desc) {
        .shader = state.shader,
        .layout = state.layout.layout,
        .vertex_layout = {
            .buffers = {
                {
                    .stride = sizeof(rc_vec2f),
                },
                {
                    .stride = sizeof(glyph_instance),
                    .per_instance = true,
                },
            },
            .attributes = {
                {
                    .location = 0,
                    .buffer_index = 0,
                    .format = RC_GFX_VERTEX_FORMAT_F32X2,
                },
                {
                    .location = 1,
                    .buffer_index = 1,
                    .offset = offsetof(glyph_instance, pos),
                    .format = RC_GFX_VERTEX_FORMAT_F32X2,
                },
                {
                    .location = 2,
                    .buffer_index = 1,
                    .offset = offsetof(glyph_instance, uv_pos),
                    .format = RC_GFX_VERTEX_FORMAT_F32X2,
                },
                {
                    .location = 3,
                    .buffer_index = 1,
                    .offset = offsetof(glyph_instance, size),
                    .format = RC_GFX_VERTEX_FORMAT_F32X2,
                },
            },
        },
        .primitive = RC_GFX_PRIMITIVE_TRIANGLE_STRIP,
        .colors = {
            {
                .format = rc_gfx_swapchain_format(),
                .blend = rc_gfx_blend_state_make_premultiplied(),
            },
        },
        .color_count = 1,
        .label = RC_STR("text pipeline"),
    });

    // the atlas view differs only in shader and blending (opaque)
    state.atlas_pip = rc_gfx_pipeline_make(&(rc_gfx_pipeline_desc) {
        .shader = state.atlas_shader,
        .layout = state.layout.layout,
        .vertex_layout = {
            .buffers = {
                {
                    .stride = sizeof(rc_vec2f),
                },
                {
                    .stride = sizeof(glyph_instance),
                    .per_instance = true,
                },
            },
            .attributes = {
                {
                    .location = 0,
                    .buffer_index = 0,
                    .format = RC_GFX_VERTEX_FORMAT_F32X2,
                },
                {
                    .location = 1,
                    .buffer_index = 1,
                    .offset = offsetof(glyph_instance, pos),
                    .format = RC_GFX_VERTEX_FORMAT_F32X2,
                },
                {
                    .location = 2,
                    .buffer_index = 1,
                    .offset = offsetof(glyph_instance, uv_pos),
                    .format = RC_GFX_VERTEX_FORMAT_F32X2,
                },
                {
                    .location = 3,
                    .buffer_index = 1,
                    .offset = offsetof(glyph_instance, size),
                    .format = RC_GFX_VERTEX_FORMAT_F32X2,
                },
            },
        },
        .primitive = RC_GFX_PRIMITIVE_TRIANGLE_STRIP,
        .colors = {
            {
                .format = rc_gfx_swapchain_format(),
            },
        },
        .color_count = 1,
        .label = RC_STR("atlas view pipeline"),
    });
}

/* ---- per frame ---- */

static void on_render(void *ctx, rc_vec2i size)
{
    (void)ctx;

    rc_gfx_begin_frame(size);
    rc_arena_reset(&state.frame_arena);

    rc_gfx_encoder *enc = rc_gfx_encoder_begin(&state.frame_arena);

    rc_gfx_encoder_pass_begin(enc, &(rc_gfx_pass_desc) {
        .colors = {
            {
                .clear_value = {0.03f, 0.03f, 0.045f, 1.0f},   // linear!
            },
        },
        .label = RC_STR("text"),
    });

    frame_uniforms frame = {
        .proj = rc_mat44f_make_ortho_2d((float)size.x, (float)size.y),
        .atlas_dim = state.atlas_dim,
        .spread = state.spread,
    };

    // the atlas view quad, opaque, below the text, at scale 1
    rc_gfx_uniform_alloc atlas_u = rc_gfx_encoder_alloc_uniforms(enc, sizeof(frame_uniforms));
    frame.scale = 1.0f;
    *(frame_uniforms *)atlas_u.ptr = frame;
    rc_gfx_encoder_set_pipeline(enc, state.atlas_pip);
    rc_gfx_encoder_set_bind_group(enc, 0, state.group0, &atlas_u.offset, 1);
    rc_gfx_encoder_set_vertex_buffer(enc, 0, state.quad_vbuf, 0);
    rc_gfx_encoder_set_vertex_buffer(enc, 1, state.atlas_quad_buf, 0);
    rc_gfx_encoder_draw(enc, &(rc_gfx_draw_desc) {
        .vertex_count = 4,
        .instance_count = 1,
    });

    // the string, alpha-blended, at TEXT_SCALE
    rc_gfx_uniform_alloc u = rc_gfx_encoder_alloc_uniforms(enc, sizeof(frame_uniforms));
    frame.scale = TEXT_SCALE;
    *(frame_uniforms *)u.ptr = frame;
    rc_gfx_encoder_set_pipeline(enc, state.pip);
    rc_gfx_encoder_set_bind_group(enc, 0, state.group0, &u.offset, 1);
    rc_gfx_encoder_set_vertex_buffer(enc, 0, state.quad_vbuf, 0);
    rc_gfx_encoder_set_vertex_buffer(enc, 1, state.instance_buf, 0);
    rc_gfx_encoder_draw(enc, &(rc_gfx_draw_desc) {
        .vertex_count = 4,
        .instance_count = state.instance_count,
    });

    rc_gfx_encoder_pass_end(enc);

    rc_gfx_cmd_buffer cb = rc_gfx_encoder_finish(enc);
    rc_gfx_submit(&cb, 1);
    rc_gfx_end_frame();
}

/* ---- main ---- */

int main(void)
{
    state.arena = rc_arena_make_default();
    state.frame_arena = rc_arena_make_default();

    rc_app_init(&(rc_app_desc) {
        .title = RC_STR("text"),
        .size = {1280, 720},
        .resizable = true,
        .callbacks = {
            .on_render = on_render,
        },
    });

    gfx_setup();

    while (rc_app_is_running()) {
        rc_app_poll();
        rc_app_request_render();
    }

    rc_gfx_shutdown();
    rc_app_destroy();
    return 0;
}
