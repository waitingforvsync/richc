/*
 * example/cubes_overlay/main.c - gfx sandbox: offscreen render targets and a
 * frosted-glass overlay.
 *
 * The rotating-cubes scene from example/cubes, but rendered through an
 * offscreen target chain instead of straight to the swapchain:
 * - render targets: the scene draws into a window-sized RGBA8_SRGB colour
 *   texture with an explicit DEPTH32F depth texture (the swapchain itself
 *   carries no depth buffer here)
 * - regional blur: only the rect's region of the scene is captured into a
 *   fixed-size chain (the rect never changes size, so these targets are
 *   created once and survive window resizes): two tent-downsample stages to
 *   quarter rect resolution (4 bilinear taps offset one source texel
 *   diagonally, a 4x4 tent footprint per stage), then a separable 7-tap
 *   gaussian (horizontal then vertical) at quarter res for extra smoothness.
 *   This is the per-overlay shape a UI layer would repeat back to front
 * - composite: the final swapchain pass presents the sharp scene, draws a
 *   frosted rectangle that samples the blurred texture at screen-space UVs
 *   and tints it toward mid grey, and renders an SDF string over it
 *   (font_atlas + premultiplied blend, as example/text)
 * - resize: the offscreen textures, targets, and the bind groups that sample
 *   them are destroyed and recreated whenever the window size changes
 *   (deferred destruction makes this safe with frames in flight)
 *
 * Run from this binary's own directory so data/fonts/Roboto-Regular.ttf
 * resolves (it is staged there by the build).
 */

#include <stddef.h>

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
#include "richc/hash.h"
#include "richc/math/mat44f.h"
#include "richc/math/quatf.h"

#define TEX_SIZE     128
#define TEX_LAYERS   4
#define GRID_DIM     4
#define CUBE_COUNT   (GRID_DIM * GRID_DIM * GRID_DIM)
#define GRID_SPACING 2.2f

#define OVERLAY_W    560.0f  /* frosted rectangle size, px */
#define OVERLAY_H    120.0f

#define PIXEL_SIZE   18.0f   /* em height, px, as rasterised into the atlas */
#define TEXT_SCALE   2.0f    /* on-screen size relative to the rasterised size */
#define ATLAS_SIZE   256     /* fits 18 px ASCII */

static const char message[] = "cubes behind frosted glass";

/* ---- data ---- */

typedef struct vertex {
    rc_vec3f pos;
    rc_vec3f normal;
    rc_vec2f uv;
} vertex;

typedef struct instance {
    rc_mat44f model;
    float     layer;
} instance;

typedef struct scene_uniforms {
    rc_mat44f view_proj;
    rc_vec4f  light_dir;     /* world space, towards the light, w unused */
} scene_uniforms;

typedef struct overlay_uniforms {
    rc_vec4f rect_ndc;       /* xy = top-left corner NDC, zw = bottom-right */
} overlay_uniforms;

typedef struct region_uniforms {
    rc_vec4f uv_rect;        /* xy = origin, zw = size, scene UV space */
} region_uniforms;

typedef struct gauss_uniforms {
    rc_vec4f dir;            /* xy = step direction in texels, zw unused */
} gauss_uniforms;

typedef struct text_uniforms {
    rc_mat44f proj;
    rc_vec2f  atlas_dim;     /* atlas size, texels */
    rc_vec2f  offset;        /* string top-left, screen pixels */
    float     spread;        /* SDF half-range, texels (rc_font.spread) */
    float     scale;         /* screen pixels per atlas texel */
} text_uniforms;

typedef struct cube_params {
    rc_vec3f pos;
    rc_vec3f axis;
    float    speed;
    float    phase;
    float    layer;
} cube_params;

/* One instance per visible glyph; the quad corners expand these in the VS. */
typedef struct glyph_instance {
    rc_vec2f pos;      /* quad top-left, screen pixels                    */
    rc_vec2f uv_pos;   /* quad top-left, normalized atlas UV              */
    rc_vec2f size;     /* quad size, pixels (== atlas texels at 100%)     */
} glyph_instance;

/* Unit cube centred on the origin, four vertices per face so each face gets
 * its own normal and UVs.  CCW winding viewed from outside; UV origin at the
 * top-left of each face (v down). */
static const vertex cube_vertices[24] = {
    /* +X */
    {.pos = {0.5f, -0.5f, 0.5f}, .normal = {1.0f, 0.0f, 0.0f}, .uv = {0.0f, 1.0f}},
    {.pos = {0.5f, -0.5f, -0.5f}, .normal = {1.0f, 0.0f, 0.0f}, .uv = {1.0f, 1.0f}},
    {.pos = {0.5f, 0.5f, -0.5f}, .normal = {1.0f, 0.0f, 0.0f}, .uv = {1.0f, 0.0f}},
    {.pos = {0.5f, 0.5f, 0.5f}, .normal = {1.0f, 0.0f, 0.0f}, .uv = {0.0f, 0.0f}},
    /* -X */
    {.pos = {-0.5f, -0.5f, -0.5f}, .normal = {-1.0f, 0.0f, 0.0f}, .uv = {0.0f, 1.0f}},
    {.pos = {-0.5f, -0.5f, 0.5f}, .normal = {-1.0f, 0.0f, 0.0f}, .uv = {1.0f, 1.0f}},
    {.pos = {-0.5f, 0.5f, 0.5f}, .normal = {-1.0f, 0.0f, 0.0f}, .uv = {1.0f, 0.0f}},
    {.pos = {-0.5f, 0.5f, -0.5f}, .normal = {-1.0f, 0.0f, 0.0f}, .uv = {0.0f, 0.0f}},
    /* +Y */
    {.pos = {-0.5f, 0.5f, 0.5f}, .normal = {0.0f, 1.0f, 0.0f}, .uv = {0.0f, 1.0f}},
    {.pos = {0.5f, 0.5f, 0.5f}, .normal = {0.0f, 1.0f, 0.0f}, .uv = {1.0f, 1.0f}},
    {.pos = {0.5f, 0.5f, -0.5f}, .normal = {0.0f, 1.0f, 0.0f}, .uv = {1.0f, 0.0f}},
    {.pos = {-0.5f, 0.5f, -0.5f}, .normal = {0.0f, 1.0f, 0.0f}, .uv = {0.0f, 0.0f}},
    /* -Y */
    {.pos = {-0.5f, -0.5f, -0.5f}, .normal = {0.0f, -1.0f, 0.0f}, .uv = {0.0f, 1.0f}},
    {.pos = {0.5f, -0.5f, -0.5f}, .normal = {0.0f, -1.0f, 0.0f}, .uv = {1.0f, 1.0f}},
    {.pos = {0.5f, -0.5f, 0.5f}, .normal = {0.0f, -1.0f, 0.0f}, .uv = {1.0f, 0.0f}},
    {.pos = {-0.5f, -0.5f, 0.5f}, .normal = {0.0f, -1.0f, 0.0f}, .uv = {0.0f, 0.0f}},
    /* +Z */
    {.pos = {-0.5f, -0.5f, 0.5f}, .normal = {0.0f, 0.0f, 1.0f}, .uv = {0.0f, 1.0f}},
    {.pos = {0.5f, -0.5f, 0.5f}, .normal = {0.0f, 0.0f, 1.0f}, .uv = {1.0f, 1.0f}},
    {.pos = {0.5f, 0.5f, 0.5f}, .normal = {0.0f, 0.0f, 1.0f}, .uv = {1.0f, 0.0f}},
    {.pos = {-0.5f, 0.5f, 0.5f}, .normal = {0.0f, 0.0f, 1.0f}, .uv = {0.0f, 0.0f}},
    /* -Z */
    {.pos = {0.5f, -0.5f, -0.5f}, .normal = {0.0f, 0.0f, -1.0f}, .uv = {0.0f, 1.0f}},
    {.pos = {-0.5f, -0.5f, -0.5f}, .normal = {0.0f, 0.0f, -1.0f}, .uv = {1.0f, 1.0f}},
    {.pos = {-0.5f, 0.5f, -0.5f}, .normal = {0.0f, 0.0f, -1.0f}, .uv = {1.0f, 0.0f}},
    {.pos = {0.5f, 0.5f, -0.5f}, .normal = {0.0f, 0.0f, -1.0f}, .uv = {0.0f, 0.0f}},
};

static const uint16_t cube_indices[36] = {
    0, 1, 2, 0, 2, 3,
    4, 5, 6, 4, 6, 7,
    8, 9, 10, 8, 10, 11,
    12, 13, 14, 12, 14, 15,
    16, 17, 18, 16, 18, 19,
    20, 21, 22, 20, 22, 23,
};

/* One oversized triangle covering the screen (no quad, no index buffer). */
static const rc_vec2f blit_vertices[3] = {
    {-1.0f, -1.0f},
    {3.0f, -1.0f},
    {-1.0f, 3.0f},
};

/* A unit quad as a triangle strip; the overlay rect and the glyph instances
 * both expand it in their vertex shaders. */
static const rc_vec2f quad_corners[4] = {
    {0.0f, 0.0f},
    {1.0f, 0.0f},
    {0.0f, 1.0f},
    {1.0f, 1.0f},
};

/* GLSL 330 bodies: no #version line (the prelude adds it), gl_Position
 * written through rc_clip.  The model matrix arrives as four per-instance
 * vec4 attributes. */
static const char scene_vs_src[] =
    "layout(location = 0) in vec3 a_pos;\n"
    "layout(location = 1) in vec3 a_normal;\n"
    "layout(location = 2) in vec2 a_uv;\n"
    "layout(location = 3) in vec4 a_model0;\n"
    "layout(location = 4) in vec4 a_model1;\n"
    "layout(location = 5) in vec4 a_model2;\n"
    "layout(location = 6) in vec4 a_model3;\n"
    "layout(location = 7) in float a_layer;\n"
    "out vec3 v_normal;\n"
    "out vec2 v_uv;\n"
    "out float v_layer;\n"
    "layout(std140) uniform SceneUniforms {\n"
    "    mat4 u_view_proj;\n"
    "    vec4 u_light_dir;\n"
    "};\n"
    "void main() {\n"
    "    mat4 model = mat4(a_model0, a_model1, a_model2, a_model3);\n"
    "    v_normal = mat3(model) * a_normal;\n"
    "    v_uv = a_uv;\n"
    "    v_layer = a_layer;\n"
    "    gl_Position = rc_clip(u_view_proj * model * vec4(a_pos, 1.0));\n"
    "}\n";

static const char scene_fs_src[] =
    "uniform sampler2DArray u_texture;\n"
    "in vec3 v_normal;\n"
    "in vec2 v_uv;\n"
    "in float v_layer;\n"
    "layout(std140) uniform SceneUniforms {\n"
    "    mat4 u_view_proj;\n"
    "    vec4 u_light_dir;\n"
    "};\n"
    "out vec4 o_color;\n"
    "void main() {\n"
    "    vec3 n = normalize(v_normal);\n"
    "    float diffuse = max(dot(n, u_light_dir.xyz), 0.0);\n"
    "    vec3 albedo = texture(u_texture, vec3(v_uv, v_layer)).rgb;\n"
    "    o_color = vec4(albedo * (0.15 + 0.85 * diffuse), 1.0);\n"
    "}\n";

static const char blit_vs_src[] =
    "layout(location = 0) in vec2 a_pos;\n"
    "out vec2 v_uv;\n"
    "void main() {\n"
    "    v_uv = a_pos * 0.5 + 0.5;\n"
    "    gl_Position = rc_clip(vec4(a_pos, 0.0, 1.0));\n"
    "}\n";

/* Fullscreen triangle again, but the interpolated UV spans only the rect's
 * window of the source texture instead of all of it: this is what captures
 * the region under the overlay into the fixed-size chain.  Paired with the
 * tent fragment shader below. */
static const char region_vs_src[] =
    "layout(location = 0) in vec2 a_pos;\n"
    "out vec2 v_uv;\n"
    "layout(std140) uniform RegionUniforms {\n"
    "    vec4 u_uv_rect;\n"
    "};\n"
    "void main() {\n"
    "    vec2 s = a_pos * 0.5 + 0.5;\n"
    "    v_uv = u_uv_rect.xy + s * u_uv_rect.zw;\n"
    "    gl_Position = rc_clip(vec4(a_pos, 0.0, 1.0));\n"
    "}\n";

/* Four bilinear taps offset one source texel diagonally: each tap already
 * averages a 2x2 block, so together they cover a 4x4 tent-weighted footprint
 * for the price of four fetches.  Serves both the region capture (with
 * region_vs) and the half -> quarter stage (with blit_vs). */
static const char downsample_fs_src[] =
    "uniform sampler2D u_tex;\n"
    "in vec2 v_uv;\n"
    "out vec4 o_color;\n"
    "void main() {\n"
    "    vec2 texel = 1.0 / vec2(textureSize(u_tex, 0));\n"
    "    vec3 c = texture(u_tex, v_uv + vec2(-1.0, -1.0) * texel).rgb\n"
    "           + texture(u_tex, v_uv + vec2(1.0, -1.0) * texel).rgb\n"
    "           + texture(u_tex, v_uv + vec2(-1.0, 1.0) * texel).rgb\n"
    "           + texture(u_tex, v_uv + vec2(1.0, 1.0) * texel).rgb;\n"
    "    o_color = vec4(c * 0.25, 1.0);\n"
    "}\n";

static const char present_fs_src[] =
    "uniform sampler2D u_tex;\n"
    "in vec2 v_uv;\n"
    "out vec4 o_color;\n"
    "void main() { o_color = texture(u_tex, v_uv); }\n";

/* One dimension of a separable 7-tap gaussian (sigma ~1.5 texels): run once
 * with u_dir = (1, 0) into a temp target and once with (0, 1) into the final
 * one.  At quarter rect resolution each texel is 4 screen pixels, so this
 * adds roughly a 6 screen-pixel soft radius on top of the tent chain. */
static const char gauss_fs_src[] =
    "uniform sampler2D u_tex;\n"
    "in vec2 v_uv;\n"
    "layout(std140) uniform GaussUniforms {\n"
    "    vec4 u_dir;\n"
    "};\n"
    "out vec4 o_color;\n"
    "void main() {\n"
    "    vec2 step = u_dir.xy / vec2(textureSize(u_tex, 0));\n"
    "    vec3 c = texture(u_tex, v_uv).rgb * 0.2707;\n"
    "    c += (texture(u_tex, v_uv + step).rgb + texture(u_tex, v_uv - step).rgb) * 0.2167;\n"
    "    c += (texture(u_tex, v_uv + 2.0 * step).rgb + texture(u_tex, v_uv - 2.0 * step).rgb) * 0.1113;\n"
    "    c += (texture(u_tex, v_uv + 3.0 * step).rgb + texture(u_tex, v_uv - 3.0 * step).rgb) * 0.0366;\n"
    "    o_color = vec4(c, 1.0);\n"
    "}\n";

/* The rect corners arrive in NDC; the blurred texture holds exactly the
 * rect's region (captured through the same NDC-derived UV window), so the
 * quad samples it with plain rect-local corners and stays aligned with the
 * sharp scene behind the rectangle. */
static const char overlay_vs_src[] =
    "layout(location = 0) in vec2 a_corner;\n"
    "out vec2 v_uv;\n"
    "layout(std140) uniform OverlayUniforms {\n"
    "    vec4 u_rect_ndc;\n"
    "};\n"
    "void main() {\n"
    "    vec2 ndc = mix(u_rect_ndc.xy, u_rect_ndc.zw, a_corner);\n"
    "    v_uv = a_corner;\n"
    "    gl_Position = rc_clip(vec4(ndc, 0.0, 1.0));\n"
    "}\n";

/* Frosted glass is optically opaque - the translucency is the blurred scene
 * showing through the sample itself - so no blending, and the tint is a mix
 * toward linear mid grey (0.216, i.e. perceptual 0.5). */
static const char overlay_fs_src[] =
    "uniform sampler2D u_blur;\n"
    "in vec2 v_uv;\n"
    "out vec4 o_color;\n"
    "void main() {\n"
    "    vec3 frosted = mix(texture(u_blur, v_uv).rgb, vec3(0.216), 0.35);\n"
    "    o_color = vec4(frosted, 1.0);\n"
    "}\n";

/* Text shaders from example/text, plus u_offset: the string is laid out once
 * at the origin at startup and repositioned per frame as the rect moves. */
static const char text_vs_src[] =
    "layout(location = 0) in vec2 a_corner;\n"
    "layout(location = 1) in vec2 i_pos;\n"
    "layout(location = 2) in vec2 i_uv_pos;\n"
    "layout(location = 3) in vec2 i_size;\n"
    "out vec2 v_uv;\n"
    "layout(std140) uniform TextUniforms {\n"
    "    mat4 u_proj;\n"
    "    vec2 u_atlas_dim;\n"
    "    vec2 u_offset;\n"
    "    float u_spread;\n"
    "    float u_scale;\n"
    "};\n"
    "void main() {\n"
    "    v_uv = i_uv_pos + a_corner * (i_size / u_atlas_dim);\n"
    "    vec2 pos = u_offset + i_pos + a_corner * i_size * u_scale;\n"
    "    gl_Position = rc_clip(u_proj * vec4(pos, 0.0, 1.0));\n"
    "}\n";

/*
 * SDF coverage via UV derivatives.  The atlas byte maps [-spread, +spread]
 * texels of signed distance onto [0, 1] around 0.5, so the sampled value
 * converts back to a distance in texels exactly.  The derivative of the
 * texel coordinate per screen pixel then rescales that to a distance in
 * screen pixels, and a one-pixel ramp around the outline antialiases it.
 */
static const char text_fs_src[] =
    "uniform sampler2D u_atlas;\n"
    "in vec2 v_uv;\n"
    "layout(std140) uniform TextUniforms {\n"
    "    mat4 u_proj;\n"
    "    vec2 u_atlas_dim;\n"
    "    vec2 u_offset;\n"
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

/* ---- state ---- */

static struct {
    rc_arena             arena;         /* persistent */
    rc_arena             frame_arena;   /* rewound every frame */

    /* scene (as example/cubes) */
    rc_gfx_buffer        vbuf;
    rc_gfx_buffer        index_buf;
    rc_gfx_buffer        instance_buf;  /* STREAM, rewritten each frame */
    rc_gfx_texture       texture;
    rc_gfx_sampler       sampler;
    rc_gfx_shader        scene_shader;
    rc_gfx_simple_layout scene_layout;
    rc_gfx_bind_group    scene_group;
    rc_gfx_pipeline      scene_pip;

    /* window-sized scene target, recreated on resize ({0} until first frame) */
    rc_vec2i             target_size;
    rc_gfx_texture       scene_color;
    rc_gfx_texture       scene_depth;
    rc_gfx_render_target scene_rt;
    rc_gfx_bind_group    scene_blit_group;   /* samples scene_color: present */
    rc_gfx_bind_group    region_group;       /* uniforms + scene_color: rect capture */

    /* fixed-size rect blur chain (the rect never changes size) */
    rc_gfx_texture       rect_half_tex;
    rc_gfx_texture       rect_quarter_tex;
    rc_gfx_texture       blur_tmp_tex;
    rc_gfx_texture       rect_blur_tex;
    rc_gfx_render_target rect_half_rt;
    rc_gfx_render_target rect_quarter_rt;
    rc_gfx_render_target blur_tmp_rt;
    rc_gfx_render_target rect_blur_rt;
    rc_gfx_bind_group    rect_half_group;    /* samples rect_half_tex */
    rc_gfx_bind_group    gauss_h_group;      /* uniforms + rect_quarter_tex */
    rc_gfx_bind_group    gauss_v_group;      /* uniforms + blur_tmp_tex */
    rc_gfx_bind_group    overlay_group;      /* uniforms + rect_blur_tex */

    /* composite */
    rc_gfx_buffer        blit_vbuf;
    rc_gfx_buffer        quad_vbuf;
    rc_gfx_sampler       linear_sampler;     /* linear clamp: blit chain, overlay, atlas */
    rc_gfx_shader        downsample_shader;
    rc_gfx_shader        region_shader;
    rc_gfx_shader        gauss_shader;
    rc_gfx_shader        present_shader;
    rc_gfx_shader        overlay_shader;
    rc_gfx_simple_layout blit_layout;
    rc_gfx_simple_layout filter_layout;      /* uniform (one vec4) + texture + sampler */
    rc_gfx_pipeline      downsample_pip;
    rc_gfx_pipeline      region_pip;
    rc_gfx_pipeline      gauss_pip;
    rc_gfx_pipeline      present_pip;
    rc_gfx_pipeline      overlay_pip;

    /* text (as example/text) */
    rc_gfx_buffer        text_instance_buf;
    uint32_t             text_instance_count;
    rc_gfx_texture       atlas_tex;
    rc_vec2f             atlas_dim;
    float                spread;
    rc_vec2f             text_size;          /* laid-out string block, scaled px */
    rc_gfx_shader        text_shader;
    rc_gfx_simple_layout text_layout;
    rc_gfx_bind_group    text_group;
    rc_gfx_pipeline      text_pip;

    cube_params          cubes[CUBE_COUNT];
    instance             instances[CUBE_COUNT];
} state;

/* ---- generated textures ---- */

static uint32_t pack_rgb(uint32_t r, uint32_t g, uint32_t b)
{
    return r | (g << 8) | (b << 16) | 0xFF000000u;
}

/* Deterministic per-index random in [0, 1). */
static float hash01(uint32_t index, uint32_t salt)
{
    uint32_t h = rc_hash_combine(rc_hash_u32(index), rc_hash_u32(salt));
    return (float)(h >> 8) * (1.0f / 16777216.0f);
}

static void fill_checker(rc_image img)
{
    for (int32_t y = 0; y < img.size.y; y++) {
        for (int32_t x = 0; x < img.size.x; x++) {
            bool a = ((x / 16 + y / 16) & 1) != 0;
            rc_image_set_pixel(img, x, y, a ? pack_rgb(230, 120, 40) : pack_rgb(45, 25, 12));
        }
    }
}

static void fill_stripes(rc_image img)
{
    for (int32_t y = 0; y < img.size.y; y++) {
        for (int32_t x = 0; x < img.size.x; x++) {
            bool a = (((x + y) / 16) & 1) != 0;
            rc_image_set_pixel(img, x, y, a ? pack_rgb(50, 190, 175) : pack_rgb(15, 45, 50));
        }
    }
}

static void fill_rings(rc_image img)
{
    float cx = (float)(img.size.x - 1) * 0.5f;
    float cy = (float)(img.size.y - 1) * 0.5f;
    for (int32_t y = 0; y < img.size.y; y++) {
        for (int32_t x = 0; x < img.size.x; x++) {
            float dx = (float)x - cx;
            float dy = (float)y - cy;
            float d = sqrtf(dx * dx + dy * dy);
            bool a = ((int32_t)(d / 12.0f) & 1) != 0;
            rc_image_set_pixel(img, x, y, a ? pack_rgb(185, 95, 220) : pack_rgb(40, 18, 50));
        }
    }
}

static void fill_noise(rc_image img)
{
    for (int32_t y = 0; y < img.size.y; y++) {
        for (int32_t x = 0; x < img.size.x; x++) {
            uint32_t cell = (uint32_t)(y / 8) * (uint32_t)(img.size.x / 8) + (uint32_t)(x / 8);
            float v = 0.25f + 0.75f * hash01(cell, 3u);
            rc_image_set_pixel(img, x, y, pack_rgb(
                (uint32_t)(40.0f * v),
                (uint32_t)(210.0f * v),
                (uint32_t)(90.0f * v)));
        }
    }
}

static rc_gfx_texture make_cube_texture(rc_arena *scratch)
{
    rc_vec2i size = {TEX_SIZE, TEX_SIZE};
    rc_gfx_texture tex = rc_gfx_texture_make(&(rc_gfx_texture_desc) {
        .dim = RC_GFX_TEXTURE_DIM_2D_ARRAY,
        .format = RC_GFX_TEXTURE_FORMAT_RGBA8_SRGB,
        .size = size,
        .depth = TEX_LAYERS,
        .mip_count = rc_gfx_mip_count(size),
        .usage = RC_GFX_TEXTURE_USAGE_SAMPLED | RC_GFX_TEXTURE_USAGE_COPY_DST,
        .label = RC_STR("cube textures"),
    });

    for (uint32_t layer = 0; layer < TEX_LAYERS; layer++) {
        rc_image img = rc_image_make(size, RC_PIXEL_FORMAT_RGBA8, scratch);
        switch (layer) {
        case 0: fill_checker(img); break;
        case 1: fill_stripes(img); break;
        case 2: fill_rings(img); break;
        case 3: fill_noise(img); break;
        default: RC_UNREACHABLE();
        }
        rc_gfx_texture_update(tex, 0, layer,
            rc_box2i_make_pos_size(rc_vec2i_make_zero(), size), img.data.view);
    }

    rc_gfx_texture_generate_mipmaps(tex);
    return tex;
}

/* ---- setup ---- */

static void cubes_setup(void)
{
    float half = (float)(GRID_DIM - 1) * 0.5f;
    uint32_t i = 0;
    for (int32_t z = 0; z < GRID_DIM; z++) {
        for (int32_t y = 0; y < GRID_DIM; y++) {
            for (int32_t x = 0; x < GRID_DIM; x++) {
                rc_vec3f axis = rc_vec3f_normalize_safe((rc_vec3f) {
                    hash01(i, 0u) * 2.0f - 1.0f,
                    hash01(i, 1u) * 2.0f - 1.0f,
                    hash01(i, 2u) * 2.0f - 1.0f
                }, 0.01f);
                if (rc_vec3f_lengthsqr(axis) == 0.0f) {
                    axis = rc_vec3f_make_unity();
                }
                state.cubes[i] = (cube_params) {
                    .pos = {
                        ((float)x - half) * GRID_SPACING,
                        ((float)y - half) * GRID_SPACING,
                        ((float)z - half) * GRID_SPACING
                    },
                    .axis = axis,
                    .speed = 0.5f + hash01(i, 4u),
                    .phase = hash01(i, 5u) * 6.2831853f,
                    .layer = (float)(rc_hash_u32(i) % TEX_LAYERS),
                };
                i++;
            }
        }
    }
}

static void scene_gfx_setup(void)
{
    state.vbuf = rc_gfx_buffer_make(&(rc_gfx_buffer_desc) {
        .size = sizeof(cube_vertices),
        .usage = RC_GFX_BUFFER_USAGE_VERTEX,
        .data = {
            .data = (const uint8_t *)cube_vertices,
            .num = sizeof(cube_vertices),
        },
        .label = RC_STR("cube vertices"),
    });

    state.index_buf = rc_gfx_buffer_make(&(rc_gfx_buffer_desc) {
        .size = sizeof(cube_indices),
        .usage = RC_GFX_BUFFER_USAGE_INDEX,
        .data = {
            .data = (const uint8_t *)cube_indices,
            .num = sizeof(cube_indices),
        },
        .label = RC_STR("cube indices"),
    });

    state.instance_buf = rc_gfx_buffer_make(&(rc_gfx_buffer_desc) {
        .size = sizeof(state.instances),
        .usage = RC_GFX_BUFFER_USAGE_VERTEX,
        .update = RC_GFX_BUFFER_UPDATE_STREAM,
        .label = RC_STR("cube instances"),
    });

    state.texture = make_cube_texture(&state.frame_arena);
    rc_arena_reset(&state.frame_arena);

    state.sampler = rc_gfx_sampler_make(&(rc_gfx_sampler_desc) {
        .min_filter = RC_GFX_FILTER_LINEAR,
        .mag_filter = RC_GFX_FILTER_LINEAR,
        .mip_filter = RC_GFX_FILTER_LINEAR,
        .address_u = RC_GFX_ADDRESS_REPEAT,
        .address_v = RC_GFX_ADDRESS_REPEAT,
        .label = RC_STR("cube sampler"),
    });

    state.scene_shader = rc_gfx_shader_make(&(rc_gfx_shader_desc) {
        .vs_source = RC_STR(scene_vs_src),
        .fs_source = RC_STR(scene_fs_src),
        .uniform_blocks = (const rc_gfx_shader_uniform_block[]) {
            {
                .glsl_name = RC_STR("SceneUniforms"),
                .group = 0,
                .binding = 0,
                .size = sizeof(scene_uniforms),
                .members = (const rc_gfx_uniform_member[]) {
                    {.name = RC_STR("u_view_proj"), .offset = offsetof(scene_uniforms, view_proj), .size = sizeof(rc_mat44f)},
                    {.name = RC_STR("u_light_dir"), .offset = offsetof(scene_uniforms, light_dir), .size = sizeof(rc_vec4f)},
                },
                .member_count = 2,
            },
        },
        .uniform_block_count = 1,
        .texture_samplers = (const rc_gfx_shader_texture_sampler_pair[]) {
            {
                .glsl_name = RC_STR("u_texture"),
                .texture_group = 0,
                .texture_binding = 1,
                .sampler_group = 0,
                .sampler_binding = 2,
            },
        },
        .texture_sampler_count = 1,
        .label = RC_STR("cube shader"),
    });

    state.scene_layout = rc_gfx_simple_layout_make(
        (const rc_gfx_bind_group_layout_entry[]) {
            {
                .binding = 0,
                .visibility = RC_GFX_STAGE_VERTEX | RC_GFX_STAGE_FRAGMENT,
                .type = RC_GFX_BINDING_UNIFORM_BUFFER,
                .has_dynamic_offset = true,
                .min_binding_size = sizeof(scene_uniforms),
            },
            {
                .binding = 1,
                .visibility = RC_GFX_STAGE_FRAGMENT,
                .type = RC_GFX_BINDING_TEXTURE,
                .texture_dim = RC_GFX_TEXTURE_DIM_2D_ARRAY,
            },
            {
                .binding = 2,
                .visibility = RC_GFX_STAGE_FRAGMENT,
                .type = RC_GFX_BINDING_SAMPLER,
            },
        },
        3,
        RC_STR("cube layout"));

    state.scene_group = rc_gfx_bind_group_make(&(rc_gfx_bind_group_desc) {
        .layout = state.scene_layout.group0,
        .entries = {
            {
                .binding = 0,
                .buffer = rc_gfx_uniform_buffer(),
                .buffer_size = sizeof(scene_uniforms),
            },
            {
                .binding = 1,
                .texture = state.texture,
            },
            {
                .binding = 2,
                .sampler = state.sampler,
            },
        },
        .entry_count = 3,
        .label = RC_STR("cube bind group"),
    });

    // identical to the cubes example's pipeline except for the target: the
    // offscreen scene target's formats are spelled literally (the swapchain
    // carries no depth buffer in this example)
    state.scene_pip = rc_gfx_pipeline_make(&(rc_gfx_pipeline_desc) {
        .shader = state.scene_shader,
        .layout = state.scene_layout.layout,
        .vertex_layout = {
            .buffers = {
                {
                    .stride = sizeof(vertex),
                },
                {
                    .stride = sizeof(instance),
                    .per_instance = true,
                },
            },
            .attributes = {
                {
                    .location = 0,
                    .buffer_index = 0,
                    .offset = offsetof(vertex, pos),
                    .format = RC_GFX_VERTEX_FORMAT_F32X3,
                },
                {
                    .location = 1,
                    .buffer_index = 0,
                    .offset = offsetof(vertex, normal),
                    .format = RC_GFX_VERTEX_FORMAT_F32X3,
                },
                {
                    .location = 2,
                    .buffer_index = 0,
                    .offset = offsetof(vertex, uv),
                    .format = RC_GFX_VERTEX_FORMAT_F32X2,
                },
                {
                    .location = 3,
                    .buffer_index = 1,
                    .offset = offsetof(instance, model),
                    .format = RC_GFX_VERTEX_FORMAT_F32X4,
                },
                {
                    .location = 4,
                    .buffer_index = 1,
                    .offset = offsetof(instance, model) + sizeof(rc_vec4f),
                    .format = RC_GFX_VERTEX_FORMAT_F32X4,
                },
                {
                    .location = 5,
                    .buffer_index = 1,
                    .offset = offsetof(instance, model) + 2 * sizeof(rc_vec4f),
                    .format = RC_GFX_VERTEX_FORMAT_F32X4,
                },
                {
                    .location = 6,
                    .buffer_index = 1,
                    .offset = offsetof(instance, model) + 3 * sizeof(rc_vec4f),
                    .format = RC_GFX_VERTEX_FORMAT_F32X4,
                },
                {
                    .location = 7,
                    .buffer_index = 1,
                    .offset = offsetof(instance, layer),
                    .format = RC_GFX_VERTEX_FORMAT_F32,
                },
            },
        },
        .index_format = RC_GFX_INDEX_FORMAT_U16,
        .cull = RC_GFX_CULL_BACK,
        .colors = {
            {
                .format = RC_GFX_TEXTURE_FORMAT_RGBA8_SRGB,
            },
        },
        .color_count = 1,
        .depth_stencil = {
            .format = RC_GFX_TEXTURE_FORMAT_DEPTH32F,
            .depth_write = true,
            .depth_compare = RC_GFX_COMPARE_GREATER_EQUAL,   /* reverse-Z */
        },
        .label = RC_STR("cube pipeline"),
    });
}

static void composite_gfx_setup(void)
{
    state.blit_vbuf = rc_gfx_buffer_make(&(rc_gfx_buffer_desc) {
        .size = sizeof(blit_vertices),
        .usage = RC_GFX_BUFFER_USAGE_VERTEX,
        .data = {
            .data = (const uint8_t *)blit_vertices,
            .num = sizeof(blit_vertices),
        },
        .label = RC_STR("blit triangle"),
    });

    state.quad_vbuf = rc_gfx_buffer_make(&(rc_gfx_buffer_desc) {
        .size = sizeof(quad_corners),
        .usage = RC_GFX_BUFFER_USAGE_VERTEX,
        .data = {
            .data = (const uint8_t *)quad_corners,
            .num = sizeof(quad_corners),
        },
        .label = RC_STR("unit quad"),
    });

    // bilinear + clamp serves the whole chain: the downsample taps, the
    // magnified blur under the rect, the present blit, and the SDF atlas
    state.linear_sampler = rc_gfx_sampler_make(&(rc_gfx_sampler_desc) {
        .min_filter = RC_GFX_FILTER_LINEAR,
        .mag_filter = RC_GFX_FILTER_LINEAR,
        .label = RC_STR("linear clamp sampler"),
    });

    state.downsample_shader = rc_gfx_shader_make(&(rc_gfx_shader_desc) {
        .vs_source = RC_STR(blit_vs_src),
        .fs_source = RC_STR(downsample_fs_src),
        .texture_samplers = (const rc_gfx_shader_texture_sampler_pair[]) {
            {
                .glsl_name = RC_STR("u_tex"),
                .texture_binding = 0,
                .sampler_binding = 1,
            },
        },
        .texture_sampler_count = 1,
        .label = RC_STR("downsample shader"),
    });

    state.present_shader = rc_gfx_shader_make(&(rc_gfx_shader_desc) {
        .vs_source = RC_STR(blit_vs_src),
        .fs_source = RC_STR(present_fs_src),
        .texture_samplers = (const rc_gfx_shader_texture_sampler_pair[]) {
            {
                .glsl_name = RC_STR("u_tex"),
                .texture_binding = 0,
                .sampler_binding = 1,
            },
        },
        .texture_sampler_count = 1,
        .label = RC_STR("present shader"),
    });

    state.region_shader = rc_gfx_shader_make(&(rc_gfx_shader_desc) {
        .vs_source = RC_STR(region_vs_src),
        .fs_source = RC_STR(downsample_fs_src),
        .uniform_blocks = (const rc_gfx_shader_uniform_block[]) {
            {
                .glsl_name = RC_STR("RegionUniforms"),
                .group = 0,
                .binding = 0,
                .size = sizeof(region_uniforms),
                .members = (const rc_gfx_uniform_member[]) {
                    {.name = RC_STR("u_uv_rect"), .offset = offsetof(region_uniforms, uv_rect), .size = sizeof(rc_vec4f)},
                },
                .member_count = 1,
            },
        },
        .uniform_block_count = 1,
        .texture_samplers = (const rc_gfx_shader_texture_sampler_pair[]) {
            {
                .glsl_name = RC_STR("u_tex"),
                .texture_binding = 1,
                .sampler_binding = 2,
            },
        },
        .texture_sampler_count = 1,
        .label = RC_STR("region capture shader"),
    });

    state.gauss_shader = rc_gfx_shader_make(&(rc_gfx_shader_desc) {
        .vs_source = RC_STR(blit_vs_src),
        .fs_source = RC_STR(gauss_fs_src),
        .uniform_blocks = (const rc_gfx_shader_uniform_block[]) {
            {
                .glsl_name = RC_STR("GaussUniforms"),
                .group = 0,
                .binding = 0,
                .size = sizeof(gauss_uniforms),
                .members = (const rc_gfx_uniform_member[]) {
                    {.name = RC_STR("u_dir"), .offset = offsetof(gauss_uniforms, dir), .size = sizeof(rc_vec4f)},
                },
                .member_count = 1,
            },
        },
        .uniform_block_count = 1,
        .texture_samplers = (const rc_gfx_shader_texture_sampler_pair[]) {
            {
                .glsl_name = RC_STR("u_tex"),
                .texture_binding = 1,
                .sampler_binding = 2,
            },
        },
        .texture_sampler_count = 1,
        .label = RC_STR("gaussian shader"),
    });

    state.overlay_shader = rc_gfx_shader_make(&(rc_gfx_shader_desc) {
        .vs_source = RC_STR(overlay_vs_src),
        .fs_source = RC_STR(overlay_fs_src),
        .uniform_blocks = (const rc_gfx_shader_uniform_block[]) {
            {
                .glsl_name = RC_STR("OverlayUniforms"),
                .group = 0,
                .binding = 0,
                .size = sizeof(overlay_uniforms),
                .members = (const rc_gfx_uniform_member[]) {
                    {.name = RC_STR("u_rect_ndc"), .offset = offsetof(overlay_uniforms, rect_ndc), .size = sizeof(rc_vec4f)},
                },
                .member_count = 1,
            },
        },
        .uniform_block_count = 1,
        .texture_samplers = (const rc_gfx_shader_texture_sampler_pair[]) {
            {
                .glsl_name = RC_STR("u_blur"),
                .texture_binding = 1,
                .sampler_binding = 2,
            },
        },
        .texture_sampler_count = 1,
        .label = RC_STR("overlay shader"),
    });

    state.blit_layout = rc_gfx_simple_layout_make(
        (const rc_gfx_bind_group_layout_entry[]) {
            {
                .binding = 0,
                .visibility = RC_GFX_STAGE_FRAGMENT,
                .type = RC_GFX_BINDING_TEXTURE,
            },
            {
                .binding = 1,
                .visibility = RC_GFX_STAGE_FRAGMENT,
                .type = RC_GFX_BINDING_SAMPLER,
            },
        },
        2,
        RC_STR("blit layout"));

    // one layout serves the region capture, both gaussian passes, and the
    // overlay: each of their uniform blocks is exactly one vec4, and the
    // uniform is read from the vertex stage in some and the fragment in others
    state.filter_layout = rc_gfx_simple_layout_make(
        (const rc_gfx_bind_group_layout_entry[]) {
            {
                .binding = 0,
                .visibility = RC_GFX_STAGE_VERTEX | RC_GFX_STAGE_FRAGMENT,
                .type = RC_GFX_BINDING_UNIFORM_BUFFER,
                .has_dynamic_offset = true,
                .min_binding_size = sizeof(rc_vec4f),
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
        RC_STR("filter layout"));

    // both downsample targets are RGBA8_SRGB, so one pipeline serves both
    // passes; only the bind group changes between them
    state.downsample_pip = rc_gfx_pipeline_make(&(rc_gfx_pipeline_desc) {
        .shader = state.downsample_shader,
        .layout = state.blit_layout.layout,
        .vertex_layout = {
            .buffers = {
                {
                    .stride = sizeof(rc_vec2f),
                },
            },
            .attributes = {
                {
                    .location = 0,
                    .buffer_index = 0,
                    .format = RC_GFX_VERTEX_FORMAT_F32X2,
                },
            },
        },
        .colors = {
            {
                .format = RC_GFX_TEXTURE_FORMAT_RGBA8_SRGB,
            },
        },
        .color_count = 1,
        .label = RC_STR("downsample pipeline"),
    });

    state.region_pip = rc_gfx_pipeline_make(&(rc_gfx_pipeline_desc) {
        .shader = state.region_shader,
        .layout = state.filter_layout.layout,
        .vertex_layout = {
            .buffers = {
                {
                    .stride = sizeof(rc_vec2f),
                },
            },
            .attributes = {
                {
                    .location = 0,
                    .buffer_index = 0,
                    .format = RC_GFX_VERTEX_FORMAT_F32X2,
                },
            },
        },
        .colors = {
            {
                .format = RC_GFX_TEXTURE_FORMAT_RGBA8_SRGB,
            },
        },
        .color_count = 1,
        .label = RC_STR("region capture pipeline"),
    });

    state.gauss_pip = rc_gfx_pipeline_make(&(rc_gfx_pipeline_desc) {
        .shader = state.gauss_shader,
        .layout = state.filter_layout.layout,
        .vertex_layout = {
            .buffers = {
                {
                    .stride = sizeof(rc_vec2f),
                },
            },
            .attributes = {
                {
                    .location = 0,
                    .buffer_index = 0,
                    .format = RC_GFX_VERTEX_FORMAT_F32X2,
                },
            },
        },
        .colors = {
            {
                .format = RC_GFX_TEXTURE_FORMAT_RGBA8_SRGB,
            },
        },
        .color_count = 1,
        .label = RC_STR("gaussian pipeline"),
    });

    state.present_pip = rc_gfx_pipeline_make(&(rc_gfx_pipeline_desc) {
        .shader = state.present_shader,
        .layout = state.blit_layout.layout,
        .vertex_layout = {
            .buffers = {
                {
                    .stride = sizeof(rc_vec2f),
                },
            },
            .attributes = {
                {
                    .location = 0,
                    .buffer_index = 0,
                    .format = RC_GFX_VERTEX_FORMAT_F32X2,
                },
            },
        },
        .colors = {
            {
                .format = rc_gfx_swapchain_format(),
            },
        },
        .color_count = 1,
        .label = RC_STR("present pipeline"),
    });

    state.overlay_pip = rc_gfx_pipeline_make(&(rc_gfx_pipeline_desc) {
        .shader = state.overlay_shader,
        .layout = state.filter_layout.layout,
        .vertex_layout = {
            .buffers = {
                {
                    .stride = sizeof(rc_vec2f),
                },
            },
            .attributes = {
                {
                    .location = 0,
                    .buffer_index = 0,
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
        .label = RC_STR("overlay pipeline"),
    });

    // the rect blur chain is sized from the overlay rect, which never
    // changes, so everything below is created once and survives window
    // resizes; only the scene-sized resources live in ensure_targets
    rc_vec2i rect_half = rc_vec2i_make((int32_t)OVERLAY_W / 2, (int32_t)OVERLAY_H / 2);
    rc_vec2i rect_quarter = rc_vec2i_make((int32_t)OVERLAY_W / 4, (int32_t)OVERLAY_H / 4);

    state.rect_half_tex = rc_gfx_texture_make(&(rc_gfx_texture_desc) {
        .format = RC_GFX_TEXTURE_FORMAT_RGBA8_SRGB,
        .size = rect_half,
        .usage = RC_GFX_TEXTURE_USAGE_RENDER_ATTACHMENT | RC_GFX_TEXTURE_USAGE_SAMPLED,
        .label = RC_STR("rect half res"),
    });
    state.rect_quarter_tex = rc_gfx_texture_make(&(rc_gfx_texture_desc) {
        .format = RC_GFX_TEXTURE_FORMAT_RGBA8_SRGB,
        .size = rect_quarter,
        .usage = RC_GFX_TEXTURE_USAGE_RENDER_ATTACHMENT | RC_GFX_TEXTURE_USAGE_SAMPLED,
        .label = RC_STR("rect quarter res"),
    });
    state.blur_tmp_tex = rc_gfx_texture_make(&(rc_gfx_texture_desc) {
        .format = RC_GFX_TEXTURE_FORMAT_RGBA8_SRGB,
        .size = rect_quarter,
        .usage = RC_GFX_TEXTURE_USAGE_RENDER_ATTACHMENT | RC_GFX_TEXTURE_USAGE_SAMPLED,
        .label = RC_STR("blur temp"),
    });
    state.rect_blur_tex = rc_gfx_texture_make(&(rc_gfx_texture_desc) {
        .format = RC_GFX_TEXTURE_FORMAT_RGBA8_SRGB,
        .size = rect_quarter,
        .usage = RC_GFX_TEXTURE_USAGE_RENDER_ATTACHMENT | RC_GFX_TEXTURE_USAGE_SAMPLED,
        .label = RC_STR("rect blur"),
    });

    state.rect_half_rt = rc_gfx_render_target_make(&(rc_gfx_render_target_desc) {
        .colors = {{.texture = state.rect_half_tex}},
        .color_count = 1,
        .label = RC_STR("rect half target"),
    });
    state.rect_quarter_rt = rc_gfx_render_target_make(&(rc_gfx_render_target_desc) {
        .colors = {{.texture = state.rect_quarter_tex}},
        .color_count = 1,
        .label = RC_STR("rect quarter target"),
    });
    state.blur_tmp_rt = rc_gfx_render_target_make(&(rc_gfx_render_target_desc) {
        .colors = {{.texture = state.blur_tmp_tex}},
        .color_count = 1,
        .label = RC_STR("blur temp target"),
    });
    state.rect_blur_rt = rc_gfx_render_target_make(&(rc_gfx_render_target_desc) {
        .colors = {{.texture = state.rect_blur_tex}},
        .color_count = 1,
        .label = RC_STR("rect blur target"),
    });

    state.rect_half_group = rc_gfx_bind_group_make(&(rc_gfx_bind_group_desc) {
        .layout = state.blit_layout.group0,
        .entries = {
            {
                .binding = 0,
                .texture = state.rect_half_tex,
            },
            {
                .binding = 1,
                .sampler = state.linear_sampler,
            },
        },
        .entry_count = 2,
        .label = RC_STR("rect half blit group"),
    });
    state.gauss_h_group = rc_gfx_bind_group_make(&(rc_gfx_bind_group_desc) {
        .layout = state.filter_layout.group0,
        .entries = {
            {
                .binding = 0,
                .buffer = rc_gfx_uniform_buffer(),
                .buffer_size = sizeof(gauss_uniforms),
            },
            {
                .binding = 1,
                .texture = state.rect_quarter_tex,
            },
            {
                .binding = 2,
                .sampler = state.linear_sampler,
            },
        },
        .entry_count = 3,
        .label = RC_STR("gauss horizontal group"),
    });
    state.gauss_v_group = rc_gfx_bind_group_make(&(rc_gfx_bind_group_desc) {
        .layout = state.filter_layout.group0,
        .entries = {
            {
                .binding = 0,
                .buffer = rc_gfx_uniform_buffer(),
                .buffer_size = sizeof(gauss_uniforms),
            },
            {
                .binding = 1,
                .texture = state.blur_tmp_tex,
            },
            {
                .binding = 2,
                .sampler = state.linear_sampler,
            },
        },
        .entry_count = 3,
        .label = RC_STR("gauss vertical group"),
    });
    state.overlay_group = rc_gfx_bind_group_make(&(rc_gfx_bind_group_desc) {
        .layout = state.filter_layout.group0,
        .entries = {
            {
                .binding = 0,
                .buffer = rc_gfx_uniform_buffer(),
                .buffer_size = sizeof(overlay_uniforms),
            },
            {
                .binding = 1,
                .texture = state.rect_blur_tex,
            },
            {
                .binding = 2,
                .sampler = state.linear_sampler,
            },
        },
        .entry_count = 3,
        .label = RC_STR("overlay bind group"),
    });
}

/*
 * Build the SDF atlas and the per-glyph instance list, as example/text, but
 * laid out at the origin: the per-frame u_offset places the finished block,
 * so the layout survives window resizes without a rebuild.
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

    // lay the string out along the baseline in SCALED screen space with its
    // block top-left at the origin; record the block size for centring
    glyph_instance instances[sizeof(message) - 1];
    uint32_t count = 0;
    rc_vec2f pen = rc_vec2f_make(0.0f, font.font.ascent * TEXT_SCALE);
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
    state.text_instance_count = count;
    state.text_size = rc_vec2f_make(pen.x,
        (font.font.ascent - font.font.descent) * TEXT_SCALE);

    state.text_instance_buf = rc_gfx_buffer_make(&(rc_gfx_buffer_desc) {
        .size = count * (uint32_t)sizeof(glyph_instance),
        .usage = RC_GFX_BUFFER_USAGE_VERTEX,
        .data = {
            .data = (const uint8_t *)instances,
            .num = count * (uint32_t)sizeof(glyph_instance),
        },
        .label = RC_STR("glyph instances"),
    });

    rc_arena_deinit(&build_arena);
}

static void text_gfx_setup(void)
{
    text_setup();

    state.text_shader = rc_gfx_shader_make(&(rc_gfx_shader_desc) {
        .vs_source = RC_STR(text_vs_src),
        .fs_source = RC_STR(text_fs_src),
        .uniform_blocks = (const rc_gfx_shader_uniform_block[]) {
            {
                .glsl_name = RC_STR("TextUniforms"),
                .group = 0,
                .binding = 0,
                .size = sizeof(text_uniforms),
                .members = (const rc_gfx_uniform_member[]) {
                    {.name = RC_STR("u_proj"), .offset = offsetof(text_uniforms, proj), .size = sizeof(rc_mat44f)},
                    {.name = RC_STR("u_atlas_dim"), .offset = offsetof(text_uniforms, atlas_dim), .size = sizeof(rc_vec2f)},
                    {.name = RC_STR("u_offset"), .offset = offsetof(text_uniforms, offset), .size = sizeof(rc_vec2f)},
                    {.name = RC_STR("u_spread"), .offset = offsetof(text_uniforms, spread), .size = sizeof(float)},
                    {.name = RC_STR("u_scale"), .offset = offsetof(text_uniforms, scale), .size = sizeof(float)},
                },
                .member_count = 5,
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

    state.text_layout = rc_gfx_simple_layout_make(
        (const rc_gfx_bind_group_layout_entry[]) {
            {
                .binding = 0,
                .visibility = RC_GFX_STAGE_VERTEX | RC_GFX_STAGE_FRAGMENT,
                .type = RC_GFX_BINDING_UNIFORM_BUFFER,
                .has_dynamic_offset = true,
                .min_binding_size = sizeof(text_uniforms),
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

    state.text_group = rc_gfx_bind_group_make(&(rc_gfx_bind_group_desc) {
        .layout = state.text_layout.group0,
        .entries = {
            {
                .binding = 0,
                .buffer = rc_gfx_uniform_buffer(),
                .buffer_size = sizeof(text_uniforms),
            },
            {
                .binding = 1,
                .texture = state.atlas_tex,
            },
            {
                .binding = 2,
                .sampler = state.linear_sampler,
            },
        },
        .entry_count = 3,
        .label = RC_STR("text bind group"),
    });

    state.text_pip = rc_gfx_pipeline_make(&(rc_gfx_pipeline_desc) {
        .shader = state.text_shader,
        .layout = state.text_layout.layout,
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
}

static void gfx_setup(void)
{
    // no swapchain_depth_format: the scene depth-tests against its own
    // offscreen DEPTH32F texture, and the composite pass needs no depth
    rc_gfx_init(&(rc_gfx_desc) {
        .arena = &state.arena,
    });

    scene_gfx_setup();
    composite_gfx_setup();
    text_gfx_setup();
}

/* ---- per frame ---- */

/*
 * Recreate the scene target when the window size changes.  Only the
 * swapchain resizes itself; the scene textures, their render target, and the
 * bind groups that sample them are ours to replace.  Deferred destruction
 * keeps the swap safe with frames still in flight.  The rect blur chain is
 * fixed-size and untouched here.
 */
static void ensure_targets(rc_vec2i size)
{
    if (rc_vec2i_is_equal(size, state.target_size)) {
        return;
    }
    if (!rc_genpool_handle_is_null(state.scene_rt.h)) {   // {0} until first created
        rc_gfx_bind_group_destroy(state.scene_blit_group);
        rc_gfx_bind_group_destroy(state.region_group);
        rc_gfx_render_target_destroy(state.scene_rt);
        rc_gfx_texture_destroy(state.scene_color);
        rc_gfx_texture_destroy(state.scene_depth);
    }

    state.scene_color = rc_gfx_texture_make(&(rc_gfx_texture_desc) {
        .format = RC_GFX_TEXTURE_FORMAT_RGBA8_SRGB,
        .size = size,
        .usage = RC_GFX_TEXTURE_USAGE_RENDER_ATTACHMENT | RC_GFX_TEXTURE_USAGE_SAMPLED,
        .label = RC_STR("scene color"),
    });
    state.scene_depth = rc_gfx_texture_make(&(rc_gfx_texture_desc) {
        .format = RC_GFX_TEXTURE_FORMAT_DEPTH32F,
        .size = size,
        .usage = RC_GFX_TEXTURE_USAGE_RENDER_ATTACHMENT,
        .label = RC_STR("scene depth"),
    });

    state.scene_rt = rc_gfx_render_target_make(&(rc_gfx_render_target_desc) {
        .colors = {{.texture = state.scene_color}},
        .color_count = 1,
        .depth_stencil = {.texture = state.scene_depth},
        .label = RC_STR("scene target"),
    });

    state.scene_blit_group = rc_gfx_bind_group_make(&(rc_gfx_bind_group_desc) {
        .layout = state.blit_layout.group0,
        .entries = {
            {
                .binding = 0,
                .texture = state.scene_color,
            },
            {
                .binding = 1,
                .sampler = state.linear_sampler,
            },
        },
        .entry_count = 2,
        .label = RC_STR("scene blit group"),
    });
    state.region_group = rc_gfx_bind_group_make(&(rc_gfx_bind_group_desc) {
        .layout = state.filter_layout.group0,
        .entries = {
            {
                .binding = 0,
                .buffer = rc_gfx_uniform_buffer(),
                .buffer_size = sizeof(region_uniforms),
            },
            {
                .binding = 1,
                .texture = state.scene_color,
            },
            {
                .binding = 2,
                .sampler = state.linear_sampler,
            },
        },
        .entry_count = 3,
        .label = RC_STR("region capture group"),
    });

    state.target_size = size;
}

static void update_instances(float t)
{
    for (uint32_t i = 0; i < CUBE_COUNT; i++) {
        cube_params c = state.cubes[i];
        rc_quatf q = rc_quatf_make_angle_axis(c.phase + c.speed * t, c.axis);
        rc_mat34f model = {
            .rot = rc_mat33f_from_quatf(q),
            .trans = c.pos,
        };
        state.instances[i] = (instance) {
            .model = rc_mat44f_from_mat34f(model),
            .layer = c.layer,
        };
    }

    rc_gfx_buffer_update(state.instance_buf, 0, (rc_view_bytes) {
        .data = (const uint8_t *)state.instances,
        .num = sizeof(state.instances),
    });
}

static void on_render(void *ctx, rc_vec2i size)
{
    (void)ctx;

    rc_gfx_begin_frame(size);
    rc_arena_reset(&state.frame_arena);
    ensure_targets(size);

    float t = (float)rc_app_time();
    update_instances(t);

    // slowly orbiting camera looking at the centre of the grid
    float angle = t * 0.15f;
    rc_vec3f eye = {sinf(angle) * 14.0f, 7.0f, cosf(angle) * 14.0f};
    rc_mat44f view = rc_mat44f_from_mat34f(
        rc_mat34f_make_lookat(eye, rc_vec3f_make_zero(), rc_vec3f_make_unity()));
    rc_mat44f proj = rc_mat44f_make_perspective_inf(
        rc_deg_to_rad(55.0f), (float)size.x / (float)size.y, 0.1f);

    // the frosted rectangle, centred, in pixels and as NDC corners (top-left
    // in xy, bottom-right in zw; pixel y down flips to NDC y up)
    rc_vec2f rect_pos = rc_vec2f_make(
        ((float)size.x - OVERLAY_W) * 0.5f,
        ((float)size.y - OVERLAY_H) * 0.5f);
    rc_vec4f rect_ndc = {
        rect_pos.x * 2.0f / (float)size.x - 1.0f,
        1.0f - rect_pos.y * 2.0f / (float)size.y,
        (rect_pos.x + OVERLAY_W) * 2.0f / (float)size.x - 1.0f,
        1.0f - (rect_pos.y + OVERLAY_H) * 2.0f / (float)size.y,
    };

    rc_gfx_encoder *enc = rc_gfx_encoder_begin(&state.frame_arena);

    rc_gfx_uniform_alloc scene_u = rc_gfx_encoder_alloc_uniforms(enc, sizeof(scene_uniforms));
    *(scene_uniforms *)scene_u.ptr = (scene_uniforms) {
        .view_proj = rc_mat44f_mul(proj, view),
        .light_dir = rc_vec4f_from_vec3f(
            rc_vec3f_normalize(rc_vec3f_make(0.4f, 1.0f, 0.6f)), 0.0f),
    };

    // the cubes scene, offscreen; the depth samples are not needed after the
    // pass, so they are discarded rather than stored
    rc_gfx_encoder_pass_begin(enc, &(rc_gfx_pass_desc) {
        .target = state.scene_rt,
        .colors = {
            {
                .clear_value = {0.012f, 0.014f, 0.02f, 1.0f},   // linear!
            },
        },
        .depth_stencil = {
            .depth_clear_value = 0.0f,   // reverse-Z far
            .depth_store_op = RC_GFX_STORE_OP_DISCARD,
        },
        .label = RC_STR("scene"),
    });

    rc_gfx_encoder_set_pipeline(enc, state.scene_pip);
    rc_gfx_encoder_set_bind_group(enc, 0, state.scene_group, &scene_u.offset, 1);
    rc_gfx_encoder_set_vertex_buffer(enc, 0, state.vbuf, 0);
    rc_gfx_encoder_set_vertex_buffer(enc, 1, state.instance_buf, 0);
    rc_gfx_encoder_set_index_buffer(enc, state.index_buf, RC_GFX_INDEX_FORMAT_U16, 0);
    rc_gfx_encoder_draw_indexed(enc, &(rc_gfx_draw_indexed_desc) {
        .index_count = 36,
        .instance_count = CUBE_COUNT,
    });

    rc_gfx_encoder_pass_end(enc);

    // rect blur chain: capture the rect's region of the scene at half rect
    // res, tent-downsample to quarter, then the separable gaussian; every
    // pixel of every target is overwritten, so nothing is loaded
    rc_gfx_uniform_alloc region_u = rc_gfx_encoder_alloc_uniforms(enc, sizeof(region_uniforms));
    *(region_uniforms *)region_u.ptr = (region_uniforms) {.uv_rect = {
        rect_ndc.x * 0.5f + 0.5f,
        rect_ndc.y * 0.5f + 0.5f,
        (rect_ndc.z - rect_ndc.x) * 0.5f,
        (rect_ndc.w - rect_ndc.y) * 0.5f,
    }};
    rc_gfx_encoder_pass_begin(enc, &(rc_gfx_pass_desc) {
        .target = state.rect_half_rt,
        .colors = {{.load_op = RC_GFX_LOAD_OP_DISCARD}},
        .label = RC_STR("capture rect"),
    });
    rc_gfx_encoder_set_pipeline(enc, state.region_pip);
    rc_gfx_encoder_set_bind_group(enc, 0, state.region_group, &region_u.offset, 1);
    rc_gfx_encoder_set_vertex_buffer(enc, 0, state.blit_vbuf, 0);
    rc_gfx_encoder_draw(enc, &(rc_gfx_draw_desc) {.vertex_count = 3});
    rc_gfx_encoder_pass_end(enc);

    rc_gfx_encoder_pass_begin(enc, &(rc_gfx_pass_desc) {
        .target = state.rect_quarter_rt,
        .colors = {{.load_op = RC_GFX_LOAD_OP_DISCARD}},
        .label = RC_STR("downsample rect"),
    });
    rc_gfx_encoder_set_pipeline(enc, state.downsample_pip);
    rc_gfx_encoder_set_bind_group(enc, 0, state.rect_half_group, NULL, 0);
    rc_gfx_encoder_set_vertex_buffer(enc, 0, state.blit_vbuf, 0);
    rc_gfx_encoder_draw(enc, &(rc_gfx_draw_desc) {.vertex_count = 3});
    rc_gfx_encoder_pass_end(enc);

    rc_gfx_uniform_alloc gauss_h_u = rc_gfx_encoder_alloc_uniforms(enc, sizeof(gauss_uniforms));
    *(gauss_uniforms *)gauss_h_u.ptr = (gauss_uniforms) {.dir = {1.0f, 0.0f, 0.0f, 0.0f}};
    rc_gfx_encoder_pass_begin(enc, &(rc_gfx_pass_desc) {
        .target = state.blur_tmp_rt,
        .colors = {{.load_op = RC_GFX_LOAD_OP_DISCARD}},
        .label = RC_STR("gauss horizontal"),
    });
    rc_gfx_encoder_set_pipeline(enc, state.gauss_pip);
    rc_gfx_encoder_set_bind_group(enc, 0, state.gauss_h_group, &gauss_h_u.offset, 1);
    rc_gfx_encoder_set_vertex_buffer(enc, 0, state.blit_vbuf, 0);
    rc_gfx_encoder_draw(enc, &(rc_gfx_draw_desc) {.vertex_count = 3});
    rc_gfx_encoder_pass_end(enc);

    rc_gfx_uniform_alloc gauss_v_u = rc_gfx_encoder_alloc_uniforms(enc, sizeof(gauss_uniforms));
    *(gauss_uniforms *)gauss_v_u.ptr = (gauss_uniforms) {.dir = {0.0f, 1.0f, 0.0f, 0.0f}};
    rc_gfx_encoder_pass_begin(enc, &(rc_gfx_pass_desc) {
        .target = state.rect_blur_rt,
        .colors = {{.load_op = RC_GFX_LOAD_OP_DISCARD}},
        .label = RC_STR("gauss vertical"),
    });
    rc_gfx_encoder_set_pipeline(enc, state.gauss_pip);
    rc_gfx_encoder_set_bind_group(enc, 0, state.gauss_v_group, &gauss_v_u.offset, 1);
    rc_gfx_encoder_set_vertex_buffer(enc, 0, state.blit_vbuf, 0);
    rc_gfx_encoder_draw(enc, &(rc_gfx_draw_desc) {.vertex_count = 3});
    rc_gfx_encoder_pass_end(enc);

    // composite to the swapchain: sharp scene, frosted rect, text
    rc_gfx_encoder_pass_begin(enc, &(rc_gfx_pass_desc) {
        .colors = {{.load_op = RC_GFX_LOAD_OP_DISCARD}},   // present covers all
        .label = RC_STR("composite"),
    });

    rc_gfx_encoder_set_pipeline(enc, state.present_pip);
    rc_gfx_encoder_set_bind_group(enc, 0, state.scene_blit_group, NULL, 0);
    rc_gfx_encoder_set_vertex_buffer(enc, 0, state.blit_vbuf, 0);
    rc_gfx_encoder_draw(enc, &(rc_gfx_draw_desc) {.vertex_count = 3});

    rc_gfx_uniform_alloc overlay_u = rc_gfx_encoder_alloc_uniforms(enc, sizeof(overlay_uniforms));
    *(overlay_uniforms *)overlay_u.ptr = (overlay_uniforms) {.rect_ndc = rect_ndc};
    rc_gfx_encoder_set_pipeline(enc, state.overlay_pip);
    rc_gfx_encoder_set_bind_group(enc, 0, state.overlay_group, &overlay_u.offset, 1);
    rc_gfx_encoder_set_vertex_buffer(enc, 0, state.quad_vbuf, 0);
    rc_gfx_encoder_draw(enc, &(rc_gfx_draw_desc) {
        .vertex_count = 4,
        .instance_count = 1,
    });

    rc_gfx_uniform_alloc text_u = rc_gfx_encoder_alloc_uniforms(enc, sizeof(text_uniforms));
    *(text_uniforms *)text_u.ptr = (text_uniforms) {
        .proj = rc_mat44f_make_ortho_2d((float)size.x, (float)size.y),
        .atlas_dim = state.atlas_dim,
        .offset = rc_vec2f_add(rect_pos, rc_vec2f_scalar_mul(
            rc_vec2f_sub(rc_vec2f_make(OVERLAY_W, OVERLAY_H), state.text_size), 0.5f)),
        .spread = state.spread,
        .scale = TEXT_SCALE,
    };
    rc_gfx_encoder_set_pipeline(enc, state.text_pip);
    rc_gfx_encoder_set_bind_group(enc, 0, state.text_group, &text_u.offset, 1);
    rc_gfx_encoder_set_vertex_buffer(enc, 0, state.quad_vbuf, 0);
    rc_gfx_encoder_set_vertex_buffer(enc, 1, state.text_instance_buf, 0);
    rc_gfx_encoder_draw(enc, &(rc_gfx_draw_desc) {
        .vertex_count = 4,
        .instance_count = state.text_instance_count,
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
        .title = RC_STR("cubes overlay"),
        .size = {1280, 720},
        .resizable = true,
        .callbacks = {
            .on_render = on_render,
        },
    });

    cubes_setup();
    gfx_setup();

    while (rc_app_is_running()) {
        rc_app_poll();
        rc_app_request_render();
    }

    rc_gfx_shutdown();
    rc_app_destroy();
    return 0;
}
