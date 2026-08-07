/*
 * example/cubes/main.c - gfx sandbox: a grid of rotating textured cubes.
 *
 * Exercises the 3D half of the gfx layer:
 * - 3D projection: reverse-Z infinite-far perspective, orbiting lookat camera
 * - depth buffer: an offscreen colour + DEPTH32F target (the swapchain target
 *   is colour-only), cleared to 0.0 and tested GREATER_EQUAL, then sampled
 *   into the swapchain by a fullscreen blit pass
 * - instancing: one cube mesh drawn CUBE_COUNT times; the model matrix and
 *   texture layer come from a per-instance vertex buffer rewritten each frame
 * - textures: a generated RGBA8_SRGB 2D array (checker, stripes, rings,
 *   noise), full mip chain, trilinear-sampled with each instance selecting
 *   its layer
 */

#include <stddef.h>

#include "richc/app/app.h"
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

typedef struct frame_uniforms {
    rc_mat44f view_proj;
    rc_vec4f  light_dir;     /* world space, towards the light, w unused */
} frame_uniforms;

typedef struct cube_params {
    rc_vec3f pos;
    rc_vec3f axis;
    float    speed;
    float    phase;
    float    layer;
} cube_params;

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

/* GLSL 330 bodies: no #version line (the prelude adds it), gl_Position
 * written through rc_clip.  The model matrix arrives as four per-instance
 * vec4 attributes. */
static const char vs_src[] =
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
    "layout(std140) uniform FrameUniforms {\n"
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

static const char fs_src[] =
    "uniform sampler2DArray u_texture;\n"
    "in vec3 v_normal;\n"
    "in vec2 v_uv;\n"
    "in float v_layer;\n"
    "layout(std140) uniform FrameUniforms {\n"
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

/* Fullscreen blit: samples the offscreen colour target into the swapchain. */
static const char blit_vs_src[] =
    "layout(location = 0) in vec2 a_pos;\n"
    "out vec2 v_uv;\n"
    "void main() {\n"
    "    v_uv = a_pos * 0.5 + 0.5;\n"
    "    gl_Position = rc_clip(vec4(a_pos, 0.0, 1.0));\n"
    "}\n";

static const char blit_fs_src[] =
    "uniform sampler2D u_tex;\n"
    "in vec2 v_uv;\n"
    "out vec4 o_color;\n"
    "void main() { o_color = texture(u_tex, v_uv); }\n";

/* One oversized triangle covering the whole screen. */
static const rc_vec2f blit_vertices[3] = {
    {-1.0f, -1.0f},
    {3.0f, -1.0f},
    {-1.0f, 3.0f},
};

/* ---- state ---- */

static struct {
    rc_arena             arena;         /* persistent */
    rc_arena             frame_arena;   /* rewound every frame */

    rc_gfx_buffer        vbuf;
    rc_gfx_buffer        index_buf;
    rc_gfx_buffer        instance_buf;  /* STREAM, rewritten each frame */
    rc_gfx_texture       texture;
    rc_gfx_sampler       sampler;
    rc_gfx_shader        shader;
    rc_gfx_simple_layout layout;
    rc_gfx_bind_group    group0;
    rc_gfx_pipeline      pip;

    rc_gfx_shader        blit_shader;
    rc_gfx_sampler       blit_sampler;
    rc_gfx_simple_layout blit_layout;
    rc_gfx_pipeline      blit_pip;
    rc_gfx_buffer        blit_vbuf;

    /* offscreen target, recreated to follow the framebuffer size */
    rc_vec2i             target_size;
    rc_gfx_texture       color;
    rc_gfx_texture       depth;
    rc_gfx_render_target rt;
    rc_gfx_bind_group    blit_group;    /* references color, so recreated with it */

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

static void gfx_setup(void)
{
    rc_gfx_init(&(rc_gfx_desc) {
        .arena = &state.arena,
    });

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
                    {.name = RC_STR("u_view_proj"), .offset = offsetof(frame_uniforms, view_proj), .size = sizeof(rc_mat44f)},
                    {.name = RC_STR("u_light_dir"), .offset = offsetof(frame_uniforms, light_dir), .size = sizeof(rc_vec4f)},
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

    state.pip = rc_gfx_pipeline_make(&(rc_gfx_pipeline_desc) {
        .shader = state.shader,
        .layout = state.layout.layout,
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

    /* blit pass objects */

    state.blit_shader = rc_gfx_shader_make(&(rc_gfx_shader_desc) {
        .vs_source = RC_STR(blit_vs_src),
        .fs_source = RC_STR(blit_fs_src),
        .texture_samplers = (const rc_gfx_shader_texture_sampler_pair[]) {
            {
                .glsl_name = RC_STR("u_tex"),
                .texture_binding = 0,
                .sampler_binding = 1,
            },
        },
        .texture_sampler_count = 1,
        .label = RC_STR("blit shader"),
    });

    state.blit_sampler = rc_gfx_sampler_make(&(rc_gfx_sampler_desc) {
        .label = RC_STR("blit sampler"),
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

    state.blit_pip = rc_gfx_pipeline_make(&(rc_gfx_pipeline_desc) {
        .shader = state.blit_shader,
        .layout = state.blit_layout.layout,
        .vertex_layout = {
            .attributes = {
                {
                    .location = 0,
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
        .label = RC_STR("blit pipeline"),
    });

    state.blit_vbuf = rc_gfx_buffer_make(&(rc_gfx_buffer_desc) {
        .size = sizeof(blit_vertices),
        .usage = RC_GFX_BUFFER_USAGE_VERTEX,
        .data = {
            .data = (const uint8_t *)blit_vertices,
            .num = sizeof(blit_vertices),
        },
        .label = RC_STR("blit vertices"),
    });
}

/* Recreate the offscreen colour + depth target (and the bind group that
 * samples it) whenever the framebuffer size changes.  Destroys are deferred
 * internally, so in-flight frames stay valid. */
static void ensure_target(rc_vec2i size)
{
    if (rc_vec2i_is_equal(size, state.target_size)) {
        return;
    }

    if (!rc_genpool_handle_is_null(state.rt.h)) {
        rc_gfx_bind_group_destroy(state.blit_group);
        rc_gfx_render_target_destroy(state.rt);
        rc_gfx_texture_destroy(state.color);
        rc_gfx_texture_destroy(state.depth);
    }

    state.color = rc_gfx_texture_make(&(rc_gfx_texture_desc) {
        .format = RC_GFX_TEXTURE_FORMAT_RGBA8_SRGB,
        .size = size,
        .usage = RC_GFX_TEXTURE_USAGE_RENDER_ATTACHMENT | RC_GFX_TEXTURE_USAGE_SAMPLED,
        .label = RC_STR("scene color"),
    });

    state.depth = rc_gfx_texture_make(&(rc_gfx_texture_desc) {
        .format = RC_GFX_TEXTURE_FORMAT_DEPTH32F,
        .size = size,
        .usage = RC_GFX_TEXTURE_USAGE_RENDER_ATTACHMENT,
        .label = RC_STR("scene depth"),
    });

    state.rt = rc_gfx_render_target_make(&(rc_gfx_render_target_desc) {
        .colors = {
            {
                .texture = state.color,
            },
        },
        .color_count = 1,
        .depth_stencil = {.texture = state.depth},
        .label = RC_STR("scene target"),
    });

    state.blit_group = rc_gfx_bind_group_make(&(rc_gfx_bind_group_desc) {
        .layout = state.blit_layout.group0,
        .entries = {
            {
                .binding = 0,
                .texture = state.color,
            },
            {
                .binding = 1,
                .sampler = state.blit_sampler,
            },
        },
        .entry_count = 2,
        .label = RC_STR("blit bind group"),
    });

    state.target_size = size;
}

/* ---- per frame ---- */

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
    ensure_target(size);

    float t = (float)rc_app_time();
    update_instances(t);

    // slowly orbiting camera looking at the centre of the grid
    float angle = t * 0.15f;
    rc_vec3f eye = {sinf(angle) * 14.0f, 7.0f, cosf(angle) * 14.0f};
    rc_mat44f view = rc_mat44f_from_mat34f(
        rc_mat34f_make_lookat(eye, rc_vec3f_make_zero(), rc_vec3f_make_unity()));
    rc_mat44f proj = rc_mat44f_make_perspective_inf(
        rc_deg_to_rad(55.0f), (float)size.x / (float)size.y, 0.1f);

    rc_gfx_encoder *enc = rc_gfx_encoder_begin(&state.frame_arena);

    rc_gfx_uniform_alloc u = rc_gfx_encoder_alloc_uniforms(enc, sizeof(frame_uniforms));
    *(frame_uniforms *)u.ptr = (frame_uniforms) {
        .view_proj = rc_mat44f_mul(proj, view),
        .light_dir = rc_vec4f_from_vec3f(
            rc_vec3f_normalize(rc_vec3f_make(0.4f, 1.0f, 0.6f)), 0.0f),
    };

    // scene pass: cubes into the offscreen target, reverse-Z depth cleared to 0
    rc_gfx_encoder_pass_begin(enc, &(rc_gfx_pass_desc) {
        .target = state.rt,
        .colors = {
            {
                .clear_value = {0.012f, 0.014f, 0.02f, 1.0f},   // linear!
            },
        },
        .depth_stencil = {
            .depth_clear_value = 0.0f,   // reverse-Z far
        },
        .label = RC_STR("scene"),
    });

    rc_gfx_encoder_set_pipeline(enc, state.pip);
    rc_gfx_encoder_set_bind_group(enc, 0, state.group0, &u.offset, 1);
    rc_gfx_encoder_set_vertex_buffer(enc, 0, state.vbuf, 0);
    rc_gfx_encoder_set_vertex_buffer(enc, 1, state.instance_buf, 0);
    rc_gfx_encoder_set_index_buffer(enc, state.index_buf, RC_GFX_INDEX_FORMAT_U16, 0);
    rc_gfx_encoder_draw_indexed(enc, &(rc_gfx_draw_indexed_desc) {
        .index_count = 36,
        .instance_count = CUBE_COUNT,
    });

    rc_gfx_encoder_pass_end(enc);

    // blit pass: sample the scene colour into the swapchain target {0}
    rc_gfx_encoder_pass_begin(enc, &(rc_gfx_pass_desc) {
        .label = RC_STR("blit"),
    });

    rc_gfx_encoder_set_pipeline(enc, state.blit_pip);
    rc_gfx_encoder_set_bind_group(enc, 0, state.blit_group, NULL, 0);
    rc_gfx_encoder_set_vertex_buffer(enc, 0, state.blit_vbuf, 0);
    rc_gfx_encoder_draw(enc, &(rc_gfx_draw_desc) {
        .vertex_count = 3,
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
        .title = RC_STR("cubes"),
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
