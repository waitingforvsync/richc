/*
 * example/hellotriangle/main.c - gfx sandbox: a coloured triangle through the full gfx
 * path (buffer, shader, layout, pipeline, uniform ring, command encoder,
 * present pass).
 *
 * Things this exercises, in the order they go wrong:
 * - the apex must appear at the TOP of the window (the canonical y chain)
 * - the background must read back around (124, 124, 124), not (51, 51, 51):
 *   the 0.2 clear value is linear and the sRGB encode must happen exactly once
 * - resizing must keep the triangle centred (the swapchain target follows
 *   rc_gfx_begin_frame's size)
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
#include "richc/math/mat44f.h"

/* ---- data ---- */

typedef struct vertex {
    rc_vec2f pos;
    rc_vec4f color;      /* linear */
} vertex;

typedef struct frame_uniforms {
    rc_mat44f mvp;
} frame_uniforms;

/* Canonical NDC: y up, so the apex is at the top of the window. */
static const vertex vertices[3] = {
    {.pos = {0.0f, 0.6f}, .color = {1.0f, 0.0f, 0.0f, 1.0f}},
    {.pos = {-0.6f, -0.5f}, .color = {0.0f, 1.0f, 0.0f, 1.0f}},
    {.pos = {0.6f, -0.5f}, .color = {0.0f, 0.0f, 1.0f, 1.0f}},
};

/* GLSL 330 bodies: no #version line (the prelude adds it), explicit attribute
 * locations, gl_Position written through rc_clip. */
static const char vs_src[] =
    "layout(location = 0) in vec2 a_pos;\n"
    "layout(location = 1) in vec4 a_color;\n"
    "out vec4 v_color;\n"
    "layout(std140) uniform FrameUniforms {\n"
    "    mat4 u_mvp;\n"
    "};\n"
    "void main() {\n"
    "    v_color = a_color;\n"
    "    gl_Position = rc_clip(u_mvp * vec4(a_pos, 0.0, 1.0));\n"
    "}\n";

static const char fs_src[] =
    "in vec4 v_color;\n"
    "out vec4 o_color;\n"
    "void main() {\n"
    "    o_color = v_color;\n"
    "}\n";

/* ---- state ---- */

static struct {
    rc_arena             arena;         /* persistent */
    rc_arena             frame_arena;   /* rewound every frame */
    rc_gfx_buffer        vbuf;
    rc_gfx_shader        shader;
    rc_gfx_simple_layout layout;
    rc_gfx_bind_group    group0;
    rc_gfx_pipeline      pip;
} state;

/* ---- setup ---- */

static void gfx_setup(void)
{
    rc_gfx_init(&(rc_gfx_desc) {
        .arena = &state.arena,
    });

    state.vbuf = rc_gfx_buffer_make(&(rc_gfx_buffer_desc) {
        .size = sizeof(vertices),
        .usage = RC_GFX_BUFFER_USAGE_VERTEX,
        .data = {
            .data = (const uint8_t *)vertices,
            .num = sizeof(vertices),
        },
        .label = RC_STR("triangle vertices"),
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
            },
        },
        .uniform_block_count = 1,
        .label = RC_STR("triangle shader"),
    });

    state.layout = rc_gfx_simple_layout_make(
        (const rc_gfx_bind_group_layout_entry[]) {
            {
                .binding = 0,
                .visibility = RC_GFX_STAGE_VERTEX,
                .type = RC_GFX_BINDING_UNIFORM_BUFFER,
                .has_dynamic_offset = true,
                .min_binding_size = sizeof(frame_uniforms),
            },
        },
        1,
        RC_STR("triangle layout"));

    // one bind group, created once: it references the uniform ring's constant
    // buffer handle; the per-frame dynamic offset selects the live data
    state.group0 = rc_gfx_bind_group_make(&(rc_gfx_bind_group_desc) {
        .layout = state.layout.group0,
        .entries = {
            {
                .binding = 0,
                .buffer = rc_gfx_uniform_buffer(),
                .buffer_size = sizeof(frame_uniforms),
            },
        },
        .entry_count = 1,
        .label = RC_STR("triangle bind group"),
    });

    state.pip = rc_gfx_pipeline_make(&(rc_gfx_pipeline_desc) {
        .shader = state.shader,
        .layout = state.layout.layout,
        .vertex_layout = {
            .buffers = {
                {
                    .stride = sizeof(vertex),
                },
            },
            .attributes = {
                {
                    .location = 0,
                    .buffer_index = 0,
                    .offset = offsetof(vertex, pos),
                    .format = RC_GFX_VERTEX_FORMAT_F32X2,
                },
                {
                    .location = 1,
                    .buffer_index = 0,
                    .offset = offsetof(vertex, color),
                    .format = RC_GFX_VERTEX_FORMAT_F32X4,
                },
            },
        },
        .colors = {
            {
                .format = rc_gfx_swapchain_format(),
            },
        },
        .color_count = 1,
        .label = RC_STR("triangle pipeline"),
    });
}

/* ---- per frame ---- */

static void on_render(void *ctx, rc_vec2i size)
{
    (void)ctx;

    rc_gfx_begin_frame(size);
    rc_arena_reset(&state.frame_arena);

    rc_gfx_encoder *enc = rc_gfx_encoder_begin(&state.frame_arena);

    // target {0} is the swapchain; load defaults to CLEAR, store to STORE
    rc_gfx_encoder_pass_begin(enc, &(rc_gfx_pass_desc) {
        .colors = {
            {
                .clear_value = {0.2f, 0.2f, 0.2f, 1.0f},   // linear!
            },
        },
        .label = RC_STR("main"),
    });

    rc_gfx_uniform_alloc u = rc_gfx_encoder_alloc_uniforms(enc, sizeof(frame_uniforms));
    *(frame_uniforms *)u.ptr = (frame_uniforms) {
        .mvp = rc_mat44f_make_identity(),
    };

    rc_gfx_encoder_set_pipeline(enc, state.pip);
    rc_gfx_encoder_set_bind_group(enc, 0, state.group0, &u.offset, 1);
    rc_gfx_encoder_set_vertex_buffer(enc, 0, state.vbuf, 0);
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
        .title = RC_STR("hello triangle"),
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
