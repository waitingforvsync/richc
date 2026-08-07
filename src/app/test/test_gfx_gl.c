/*
 * test_gfx_gl.c - gfx tests that need a live GL context.
 *
 * Each test gets a fresh hidden window (and so a fresh context) from the
 * group fixture, initialises the device itself (colour space varies by
 * test), and reads results back from the default framebuffer after the
 * present pass with glReadPixels.  These automate the design's acceptance
 * criteria: the sRGB 188/128 readback, the y-orientation chain, decode-
 * before-filter, reverse-Z depth, MSAA resolve, and the uniform ring.
 *
 * glReadPixels returns rows bottom-first, so a canonical row r from the top
 * of a target of height h reads GL y = h - 1 - r.
 */

#include <glad/gl.h>

#include "richc/app/app.h"
#include "richc/arena.h"
#include "richc/gfx/bindings.h"
#include "richc/gfx/buffer.h"
#include "richc/gfx/color.h"
#include "richc/gfx/encoder.h"
#include "richc/gfx/gfx.h"
#include "richc/gfx/pass.h"
#include "richc/gfx/pipeline.h"
#include "richc/gfx/shader.h"
#include "richc/gfx/texture.h"
#include "richc/image/image.h"
#include "richc/math/mat44f.h"
#include "richc/test.h"

RC_TEST_GROUP_DATA(gfx_gl) {
    rc_arena arena;
    rc_arena frame;
};

RC_TEST_GROUP_INIT(gfx_gl, fix)
{
    rc_app_init(&(rc_app_desc) {
        .title = RC_STR("gfx test"),
        .size = {256, 256},
        .hidden = true,
    });
    fix->arena = rc_arena_make_default();
    fix->frame = rc_arena_make_default();
}

RC_TEST_GROUP_DEINIT(gfx_gl, fix)
{
    rc_arena_deinit(&fix->frame);
    rc_arena_deinit(&fix->arena);
    rc_app_destroy();
}

/* Read one pixel of the default framebuffer at canonical (x, r-from-top). */
static uint32_t read_window_pixel(int32_t x, int32_t row_from_top)
{
    rc_vec2i size = rc_app_size();
    uint8_t px[4] = {0};
    glReadPixels(x, size.y - 1 - row_from_top, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
    return (uint32_t)px[0] | (uint32_t)px[1] << 8 | (uint32_t)px[2] << 16 | (uint32_t)px[3] << 24;
}

static int32_t channel_r(uint32_t px) { return (int32_t)(px & 0xFFu); }
static int32_t channel_g(uint32_t px) { return (int32_t)((px >> 8) & 0xFFu); }
static int32_t channel_b(uint32_t px) { return (int32_t)((px >> 16) & 0xFFu); }

static bool near_u8(int32_t value, int32_t expected, int32_t tolerance)
{
    int32_t diff = value - expected;
    return diff <= tolerance && diff >= -tolerance;
}

/* Run one frame that just clears the swapchain to the given linear colour. */
static void run_clear_frame(rc_arena *frame, rc_vec4f linear)
{
    rc_gfx_begin_frame(rc_app_size());
    rc_arena_reset(frame);
    rc_gfx_encoder *enc = rc_gfx_encoder_begin(frame);
    rc_gfx_encoder_pass_begin(enc, &(rc_gfx_pass_desc) {
        .colors = {
            {
                .clear_value = linear,
            },
        },
    });
    rc_gfx_encoder_pass_end(enc);
    rc_gfx_cmd_buffer cb = rc_gfx_encoder_finish(enc);
    rc_gfx_submit(&cb, 1);
    rc_gfx_end_frame();
}

RC_TEST_STEP(gfx_gl, device_queries, fix)
{
    rc_gfx_init(&(rc_gfx_desc) {.arena = &fix->arena});

    RC_CHECK((uint32_t)rc_gfx_swapchain_format(), ==, (uint32_t)RC_GFX_TEXTURE_FORMAT_RGBA8_SRGB);
    rc_gfx_features features = rc_gfx_features_query();
    RC_CHECK_TRUE(features.storage_buffers_via_tbo);
    RC_CHECK_FALSE(features.compute);
    RC_CHECK_TRUE(features.timer_queries);
    // ARB_clip_control is exposed on essentially every desktop driver and the
    // vendored glad now loads it; if this fires, the native [0,1] depth path
    // regressed (or the machine's driver genuinely lacks the extension)
    RC_CHECK_TRUE(features.native_depth_zero_to_one);

    rc_gfx_limits limits = rc_gfx_limits_query();
    RC_CHECK_TRUE(limits.max_texture_size_2d >= 1024);
    RC_CHECK_TRUE(limits.uniform_buffer_offset_alignment <= RC_GFX_UNIFORM_ALIGN);
    RC_CHECK_TRUE(limits.max_msaa_samples >= 4);

    uint32_t caps = rc_gfx_format_caps_query(RC_GFX_TEXTURE_FORMAT_RGBA8_SRGB);
    RC_CHECK_TRUE(caps & RC_GFX_FORMAT_CAP_RENDER);
    RC_CHECK_TRUE(caps & RC_GFX_FORMAT_CAP_FILTER);
    RC_CHECK_FALSE(rc_gfx_format_caps_query(RC_GFX_TEXTURE_FORMAT_RGBA32_UINT) & RC_GFX_FORMAT_CAP_FILTER);
    RC_CHECK(rc_gfx_backend_name(), ==, RC_STR("OpenGL 3.3"));

    rc_gfx_shutdown();
}

RC_TEST_STEP(gfx_gl, clear_srgb_readback, fix)
{
    // linear 0.5 must present as 188: 0.5 -> 0.7354 encoded -> 187.5.  A
    // result of 128 means the encode is missing entirely.
    rc_gfx_init(&(rc_gfx_desc) {.arena = &fix->arena});
    run_clear_frame(&fix->frame, rc_vec4f_make(0.5f, 0.5f, 0.5f, 1.0f));
    uint32_t px = read_window_pixel(128, 128);
    RC_CHECK_TRUE(near_u8(channel_r(px), 188, 2));
    RC_CHECK_TRUE(near_u8(channel_g(px), 188, 2));
    RC_CHECK_TRUE(near_u8(channel_b(px), 188, 2));
    rc_gfx_shutdown();
}

RC_TEST_STEP(gfx_gl, clear_linear_readback, fix)
{
    // LINEAR colour space: no encode anywhere, 0.5 -> 128 (the bit-exact path)
    rc_gfx_init(&(rc_gfx_desc) {
        .arena = &fix->arena,
        .color_space = RC_GFX_COLOR_SPACE_LINEAR,
    });
    RC_CHECK((uint32_t)rc_gfx_swapchain_format(), ==, (uint32_t)RC_GFX_TEXTURE_FORMAT_RGBA8_UNORM);
    run_clear_frame(&fix->frame, rc_vec4f_make(0.5f, 0.5f, 0.5f, 1.0f));
    uint32_t px = read_window_pixel(128, 128);
    RC_CHECK_TRUE(near_u8(channel_r(px), 128, 1));
    rc_gfx_shutdown();
}

/* A pipeline drawing a solid colour over the whole clip space, no bindings. */
static rc_gfx_pipeline make_solid_pipeline(rc_gfx_shader *out_shader,
                                           rc_gfx_pipeline_layout *out_layout,
                                           rc_gfx_texture_format target_format,
                                           const char *fs_body)
{
    static const char vs_src[] =
        "layout(location = 0) in vec2 a_pos;\n"
        "void main() {\n"
        "    gl_Position = rc_clip(vec4(a_pos, 0.0, 1.0));\n"
        "}\n";
    *out_shader = rc_gfx_shader_make(&(rc_gfx_shader_desc) {
        .vs_source = RC_STR(vs_src),
        .fs_source = rc_str_from_cstr(fs_body),
    });
    *out_layout = rc_gfx_pipeline_layout_make(&(rc_gfx_pipeline_layout_desc) {0});
    return rc_gfx_pipeline_make(&(rc_gfx_pipeline_desc) {
        .shader = *out_shader,
        .layout = *out_layout,
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
                .format = target_format,
            },
        },
        .color_count = 1,
    });
}

/* One triangle covering all of clip space (canonical NDC). */
static rc_gfx_buffer make_fullscreen_vbuf(void)
{
    static const float verts[6] = {-1.0f, -1.0f, 3.0f, -1.0f, -1.0f, 3.0f};
    return rc_gfx_buffer_make(&(rc_gfx_buffer_desc) {
        .size = sizeof(verts),
        .usage = RC_GFX_BUFFER_USAGE_VERTEX,
        .data = {.data = (const uint8_t *)verts, .num = sizeof(verts)},
    });
}

RC_TEST_STEP(gfx_gl, scissored_draw_top_left, fix)
{
    // the design's M0 acceptance criterion: painting the canonical top-left
    // quadrant must land in the top-left of the window, validating the whole
    // y chain (rc_clip negation, FBO, pass-through scissor, present flip)
    rc_gfx_init(&(rc_gfx_desc) {.arena = &fix->arena});
    rc_gfx_shader shader = {0};
    rc_gfx_pipeline_layout layout = {0};
    rc_gfx_pipeline pip = make_solid_pipeline(&shader, &layout, rc_gfx_swapchain_format(),
        "out vec4 o_color;\n"
        "void main() { o_color = vec4(1.0, 1.0, 1.0, 1.0); }\n");
    rc_gfx_buffer vbuf = make_fullscreen_vbuf();

    rc_vec2i size = rc_app_size();
    rc_gfx_begin_frame(size);
    rc_gfx_encoder *enc = rc_gfx_encoder_begin(&fix->frame);
    rc_gfx_encoder_pass_begin(enc, &(rc_gfx_pass_desc) {0});   // clears to black
    rc_gfx_encoder_set_scissor(enc, rc_box2i_make_pos_size(
        rc_vec2i_make_zero(), rc_vec2i_make(size.x / 2, size.y / 2)));
    rc_gfx_encoder_set_pipeline(enc, pip);
    rc_gfx_encoder_set_vertex_buffer(enc, 0, vbuf, 0);
    rc_gfx_encoder_draw(enc, &(rc_gfx_draw_desc) {.vertex_count = 3});
    rc_gfx_encoder_pass_end(enc);
    rc_gfx_cmd_buffer cb = rc_gfx_encoder_finish(enc);
    rc_gfx_submit(&cb, 1);
    rc_gfx_end_frame();

    RC_CHECK(channel_r(read_window_pixel(4, 4)), ==, 255);                       // top-left: painted
    RC_CHECK(channel_r(read_window_pixel(4, size.y - 4)), ==, 0);                // bottom-left: background
    RC_CHECK(channel_r(read_window_pixel(size.x - 4, 4)), ==, 0);                // top-right: background
    RC_CHECK(channel_r(read_window_pixel(size.x - 4, size.y - 4)), ==, 0);       // bottom-right: background

    rc_gfx_shutdown();
}

RC_TEST_STEP(gfx_gl, triangle_uniforms, fix)
{
    // the hello-triangle path end to end: vertex pulling, the uniform ring
    // with a dynamic offset, and the canonical orientation (apex at the top)
    rc_gfx_init(&(rc_gfx_desc) {.arena = &fix->arena});

    typedef struct vertex {
        rc_vec2f pos;
        rc_vec4f color;
    } vertex;
    typedef struct frame_uniforms {
        rc_mat44f mvp;
    } frame_uniforms;

    static const vertex vertices[3] = {
        {.pos = {0.0f, 0.9f}, .color = {1.0f, 0.0f, 0.0f, 1.0f}},
        {.pos = {-0.9f, -0.9f}, .color = {1.0f, 0.0f, 0.0f, 1.0f}},
        {.pos = {0.9f, -0.9f}, .color = {1.0f, 0.0f, 0.0f, 1.0f}},
    };
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
        "void main() { o_color = v_color; }\n";

    rc_gfx_buffer vbuf = rc_gfx_buffer_make(&(rc_gfx_buffer_desc) {
        .size = sizeof(vertices),
        .usage = RC_GFX_BUFFER_USAGE_VERTEX,
        .data = {.data = (const uint8_t *)vertices, .num = sizeof(vertices)},
    });
    rc_gfx_shader shader = rc_gfx_shader_make(&(rc_gfx_shader_desc) {
        .vs_source = RC_STR(vs_src),
        .fs_source = RC_STR(fs_src),
        .uniform_blocks = (const rc_gfx_shader_uniform_block[]) {
            {
                .glsl_name = RC_STR("FrameUniforms"),
                .binding = 0,
                .size = sizeof(frame_uniforms),
                .members = (const rc_gfx_uniform_member[]) {
                    {.name = RC_STR("u_mvp"), .offset = offsetof(frame_uniforms, mvp), .size = sizeof(rc_mat44f)},
                },
                .member_count = 1,
            },
        },
        .uniform_block_count = 1,
    });
    rc_gfx_simple_layout layout = rc_gfx_simple_layout_make(
        (const rc_gfx_bind_group_layout_entry[]) {
            {
                .binding = 0,
                .visibility = RC_GFX_STAGE_VERTEX,
                .type = RC_GFX_BINDING_UNIFORM_BUFFER,
                .has_dynamic_offset = true,
                .min_binding_size = sizeof(frame_uniforms),
            },
        },
        1, RC_STR("triangle layout"));
    rc_gfx_bind_group group0 = rc_gfx_bind_group_make(&(rc_gfx_bind_group_desc) {
        .layout = layout.group0,
        .entries = {
            {
                .binding = 0,
                .buffer = rc_gfx_uniform_buffer(),
                .buffer_size = sizeof(frame_uniforms),
            },
        },
        .entry_count = 1,
    });
    rc_gfx_pipeline pip = rc_gfx_pipeline_make(&(rc_gfx_pipeline_desc) {
        .shader = shader,
        .layout = layout.layout,
        .vertex_layout = {
            .attributes = {
                {.location = 0, .format = RC_GFX_VERTEX_FORMAT_F32X2},
                {.location = 1, .format = RC_GFX_VERTEX_FORMAT_F32X4},
            },
        },
        .colors = {
            {
                .format = rc_gfx_swapchain_format(),
            },
        },
        .color_count = 1,
    });

    rc_vec2i size = rc_app_size();
    rc_gfx_begin_frame(size);
    rc_gfx_encoder *enc = rc_gfx_encoder_begin(&fix->frame);
    rc_gfx_encoder_pass_begin(enc, &(rc_gfx_pass_desc) {0});
    rc_gfx_uniform_alloc u = rc_gfx_encoder_alloc_uniforms(enc, sizeof(frame_uniforms));
    *(frame_uniforms *)u.ptr = (frame_uniforms) {.mvp = rc_mat44f_make_identity()};
    rc_gfx_encoder_set_pipeline(enc, pip);
    rc_gfx_encoder_set_bind_group(enc, 0, group0, &u.offset, 1);
    rc_gfx_encoder_set_vertex_buffer(enc, 0, vbuf, 0);
    rc_gfx_encoder_draw(enc, &(rc_gfx_draw_desc) {.vertex_count = 3});
    rc_gfx_encoder_pass_end(enc);
    rc_gfx_cmd_buffer cb = rc_gfx_encoder_finish(enc);
    rc_gfx_submit(&cb, 1);
    rc_gfx_end_frame();

    // apex at the top: the centre column near the top is red, the top corners
    // are background, and the bottom centre (inside the base) is red
    uint32_t apex = read_window_pixel(size.x / 2, size.y / 10);
    RC_CHECK(channel_r(apex), ==, 255);
    RC_CHECK(channel_g(apex), ==, 0);
    RC_CHECK(channel_r(read_window_pixel(4, 4)), ==, 0);
    RC_CHECK(channel_r(read_window_pixel(size.x - 4, 4)), ==, 0);
    RC_CHECK(channel_r(read_window_pixel(size.x / 2, size.y - size.y / 10)), ==, 255);

    // handle bookkeeping: destroying and recreating a buffer reuses the slot
    // under a bumped generation, so the stale handle can never alias
    rc_gfx_buffer_destroy(vbuf);
    rc_gfx_buffer replacement = rc_gfx_buffer_make(&(rc_gfx_buffer_desc) {
        .size = sizeof(vertices),
        .usage = RC_GFX_BUFFER_USAGE_VERTEX,
        .data = {.data = (const uint8_t *)vertices, .num = sizeof(vertices)},
    });
    RC_CHECK(rc_genpool_handle_index(replacement.h), ==, rc_genpool_handle_index(vbuf.h));
    RC_CHECK(rc_genpool_handle_gen(replacement.h), ==, rc_genpool_handle_gen(vbuf.h) + 1);

    rc_gfx_shutdown();
}

RC_TEST_STEP(gfx_gl, texture_decode_before_filter, fix)
{
    // a 2x1 sRGB texture with texels 0 and 255, sampled with linear filtering
    // at u = 0.5: correct hardware decode-before-filter averages the LINEAR
    // values and presents ~188; filtering encoded values would present ~128
    rc_gfx_init(&(rc_gfx_desc) {.arena = &fix->arena});

    static const uint8_t texels[8] = {0, 0, 0, 255, 255, 255, 255, 255};
    rc_gfx_texture tex = rc_gfx_texture_make(&(rc_gfx_texture_desc) {
        .format = RC_GFX_TEXTURE_FORMAT_RGBA8_SRGB,
        .size = {2, 1},
        .data = {
            .subresources = (const rc_view_bytes[]) {
                {.data = texels, .num = sizeof(texels)},
            },
            .count = 1,
        },
    });
    rc_gfx_sampler smp = rc_gfx_sampler_make(&(rc_gfx_sampler_desc) {
        .min_filter = RC_GFX_FILTER_LINEAR,
        .mag_filter = RC_GFX_FILTER_LINEAR,
    });

    static const char vs_src[] =
        "layout(location = 0) in vec2 a_pos;\n"
        "void main() { gl_Position = rc_clip(vec4(a_pos, 0.0, 1.0)); }\n";
    static const char fs_src[] =
        "uniform sampler2D u_tex;\n"
        "out vec4 o_color;\n"
        "void main() { o_color = texture(u_tex, vec2(0.5, 0.5)); }\n";
    rc_gfx_shader shader = rc_gfx_shader_make(&(rc_gfx_shader_desc) {
        .vs_source = RC_STR(vs_src),
        .fs_source = RC_STR(fs_src),
        .texture_samplers = (const rc_gfx_shader_texture_sampler_pair[]) {
            {
                .glsl_name = RC_STR("u_tex"),
                .texture_binding = 0,
                .sampler_binding = 1,
            },
        },
        .texture_sampler_count = 1,
    });
    rc_gfx_simple_layout layout = rc_gfx_simple_layout_make(
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
        2, RC_STR("texture layout"));
    rc_gfx_bind_group group0 = rc_gfx_bind_group_make(&(rc_gfx_bind_group_desc) {
        .layout = layout.group0,
        .entries = {
            {.binding = 0, .texture = tex},
            {.binding = 1, .sampler = smp},
        },
        .entry_count = 2,
    });
    rc_gfx_pipeline pip = rc_gfx_pipeline_make(&(rc_gfx_pipeline_desc) {
        .shader = shader,
        .layout = layout.layout,
        .vertex_layout = {
            .attributes = {
                {.location = 0, .format = RC_GFX_VERTEX_FORMAT_F32X2},
            },
        },
        .colors = {
            {
                .format = rc_gfx_swapchain_format(),
            },
        },
        .color_count = 1,
    });
    rc_gfx_buffer vbuf = make_fullscreen_vbuf();

    rc_gfx_begin_frame(rc_app_size());
    rc_gfx_encoder *enc = rc_gfx_encoder_begin(&fix->frame);
    rc_gfx_encoder_pass_begin(enc, &(rc_gfx_pass_desc) {0});
    rc_gfx_encoder_set_pipeline(enc, pip);
    rc_gfx_encoder_set_bind_group(enc, 0, group0, NULL, 0);
    rc_gfx_encoder_set_vertex_buffer(enc, 0, vbuf, 0);
    rc_gfx_encoder_draw(enc, &(rc_gfx_draw_desc) {.vertex_count = 3});
    rc_gfx_encoder_pass_end(enc);
    rc_gfx_cmd_buffer cb = rc_gfx_encoder_finish(enc);
    rc_gfx_submit(&cb, 1);
    rc_gfx_end_frame();

    uint32_t px = read_window_pixel(128, 128);
    RC_CHECK_TRUE(near_u8(channel_r(px), 188, 3));

    rc_gfx_shutdown();
}

RC_TEST_STEP(gfx_gl, offscreen_depth_reverse_z, fix)
{
    // reverse-Z on an offscreen target: clear depth to 0, GREATER_EQUAL
    // compare, draw red near (depth 0.75) then green far (depth 0.25); the
    // far draw must be rejected.  The result is sampled into the swapchain.
    rc_gfx_init(&(rc_gfx_desc) {.arena = &fix->arena});

    rc_gfx_texture color = rc_gfx_texture_make(&(rc_gfx_texture_desc) {
        .format = RC_GFX_TEXTURE_FORMAT_RGBA8_UNORM,
        .size = {64, 64},
        .usage = RC_GFX_TEXTURE_USAGE_RENDER_ATTACHMENT | RC_GFX_TEXTURE_USAGE_SAMPLED,
    });
    rc_gfx_texture depth = rc_gfx_texture_make(&(rc_gfx_texture_desc) {
        .format = RC_GFX_TEXTURE_FORMAT_DEPTH32F,
        .size = {64, 64},
        .usage = RC_GFX_TEXTURE_USAGE_RENDER_ATTACHMENT,
    });
    rc_gfx_render_target rt = rc_gfx_render_target_make(&(rc_gfx_render_target_desc) {
        .colors = {
            {
                .texture = color,
            },
        },
        .color_count = 1,
        .depth_stencil = {.texture = depth},
    });
    RC_CHECK(rc_gfx_render_target_size(rt), ==, rc_vec2i_make(64, 64));

    // scene pipeline: solid colour and depth from a uniform block
    typedef struct draw_uniforms {
        rc_vec4f color;
        float    z;
        float    pad[3];
    } draw_uniforms;
    static const char vs_src[] =
        "layout(location = 0) in vec2 a_pos;\n"
        "layout(std140) uniform DrawUniforms {\n"
        "    vec4 u_color;\n"
        "    float u_z;\n"
        "};\n"
        "void main() { gl_Position = rc_clip(vec4(a_pos, u_z, 1.0)); }\n";
    static const char fs_src[] =
        "layout(std140) uniform DrawUniforms {\n"
        "    vec4 u_color;\n"
        "    float u_z;\n"
        "};\n"
        "out vec4 o_color;\n"
        "void main() { o_color = u_color; }\n";
    rc_gfx_shader scene_shader = rc_gfx_shader_make(&(rc_gfx_shader_desc) {
        .vs_source = RC_STR(vs_src),
        .fs_source = RC_STR(fs_src),
        .uniform_blocks = (const rc_gfx_shader_uniform_block[]) {
            {
                .glsl_name = RC_STR("DrawUniforms"),
                .binding = 0,
                .size = sizeof(draw_uniforms),
                .members = (const rc_gfx_uniform_member[]) {
                    {.name = RC_STR("u_color"), .offset = offsetof(draw_uniforms, color), .size = sizeof(rc_vec4f)},
                    {.name = RC_STR("u_z"), .offset = offsetof(draw_uniforms, z), .size = sizeof(float)},
                },
                .member_count = 2,
            },
        },
        .uniform_block_count = 1,
    });
    rc_gfx_simple_layout scene_layout = rc_gfx_simple_layout_make(
        (const rc_gfx_bind_group_layout_entry[]) {
            {
                .binding = 0,
                .visibility = RC_GFX_STAGE_VERTEX | RC_GFX_STAGE_FRAGMENT,
                .type = RC_GFX_BINDING_UNIFORM_BUFFER,
                .has_dynamic_offset = true,
                .min_binding_size = sizeof(draw_uniforms),
            },
        },
        1, RC_STR("scene layout"));
    rc_gfx_bind_group scene_group = rc_gfx_bind_group_make(&(rc_gfx_bind_group_desc) {
        .layout = scene_layout.group0,
        .entries = {
            {
                .binding = 0,
                .buffer = rc_gfx_uniform_buffer(),
                .buffer_size = sizeof(draw_uniforms),
            },
        },
        .entry_count = 1,
    });
    rc_gfx_pipeline scene_pip = rc_gfx_pipeline_make(&(rc_gfx_pipeline_desc) {
        .shader = scene_shader,
        .layout = scene_layout.layout,
        .vertex_layout = {
            .attributes = {
                {.location = 0, .format = RC_GFX_VERTEX_FORMAT_F32X2},
            },
        },
        .colors = {
            {
                .format = RC_GFX_TEXTURE_FORMAT_RGBA8_UNORM,
            },
        },
        .color_count = 1,
        .depth_stencil = {
            .format = RC_GFX_TEXTURE_FORMAT_DEPTH32F,
            .depth_write = true,
            .depth_compare = RC_GFX_COMPARE_GREATER_EQUAL,
        },
    });

    // present pipeline: sample the offscreen colour into the swapchain
    static const char blit_fs[] =
        "uniform sampler2D u_tex;\n"
        "in vec2 v_uv;\n"
        "out vec4 o_color;\n"
        "void main() { o_color = texture(u_tex, v_uv); }\n";
    static const char blit_vs[] =
        "layout(location = 0) in vec2 a_pos;\n"
        "out vec2 v_uv;\n"
        "void main() {\n"
        "    v_uv = a_pos * 0.5 + 0.5;\n"
        "    gl_Position = rc_clip(vec4(a_pos, 0.0, 1.0));\n"
        "}\n";
    rc_gfx_shader blit_shader = rc_gfx_shader_make(&(rc_gfx_shader_desc) {
        .vs_source = RC_STR(blit_vs),
        .fs_source = RC_STR(blit_fs),
        .texture_samplers = (const rc_gfx_shader_texture_sampler_pair[]) {
            {
                .glsl_name = RC_STR("u_tex"),
                .texture_binding = 0,
                .sampler_binding = 1,
            },
        },
        .texture_sampler_count = 1,
    });
    rc_gfx_sampler smp = rc_gfx_sampler_make(&(rc_gfx_sampler_desc) {0});
    rc_gfx_simple_layout blit_layout = rc_gfx_simple_layout_make(
        (const rc_gfx_bind_group_layout_entry[]) {
            {.binding = 0, .visibility = RC_GFX_STAGE_FRAGMENT, .type = RC_GFX_BINDING_TEXTURE},
            {.binding = 1, .visibility = RC_GFX_STAGE_FRAGMENT, .type = RC_GFX_BINDING_SAMPLER},
        },
        2, RC_STR("blit layout"));
    rc_gfx_bind_group blit_group = rc_gfx_bind_group_make(&(rc_gfx_bind_group_desc) {
        .layout = blit_layout.group0,
        .entries = {
            {.binding = 0, .texture = color},
            {.binding = 1, .sampler = smp},
        },
        .entry_count = 2,
    });
    rc_gfx_pipeline blit_pip = rc_gfx_pipeline_make(&(rc_gfx_pipeline_desc) {
        .shader = blit_shader,
        .layout = blit_layout.layout,
        .vertex_layout = {
            .attributes = {
                {.location = 0, .format = RC_GFX_VERTEX_FORMAT_F32X2},
            },
        },
        .colors = {
            {
                .format = rc_gfx_swapchain_format(),
            },
        },
        .color_count = 1,
    });
    rc_gfx_buffer vbuf = make_fullscreen_vbuf();

    rc_gfx_begin_frame(rc_app_size());
    rc_gfx_encoder *enc = rc_gfx_encoder_begin(&fix->frame);

    rc_gfx_encoder_pass_begin(enc, &(rc_gfx_pass_desc) {
        .target = rt,
        .depth_stencil = {
            .depth_clear_value = 0.0f,   // reverse-Z far
        },
    });
    rc_gfx_encoder_set_pipeline(enc, scene_pip);
    rc_gfx_encoder_set_vertex_buffer(enc, 0, vbuf, 0);
    rc_gfx_uniform_alloc near_u = rc_gfx_encoder_alloc_uniforms(enc, sizeof(draw_uniforms));
    *(draw_uniforms *)near_u.ptr = (draw_uniforms) {
        .color = {1.0f, 0.0f, 0.0f, 1.0f},
        .z = 0.75f,
    };
    rc_gfx_encoder_set_bind_group(enc, 0, scene_group, &near_u.offset, 1);
    rc_gfx_encoder_draw(enc, &(rc_gfx_draw_desc) {.vertex_count = 3});
    rc_gfx_uniform_alloc far_u = rc_gfx_encoder_alloc_uniforms(enc, sizeof(draw_uniforms));
    *(draw_uniforms *)far_u.ptr = (draw_uniforms) {
        .color = {0.0f, 1.0f, 0.0f, 1.0f},
        .z = 0.25f,
    };
    rc_gfx_encoder_set_bind_group(enc, 0, scene_group, &far_u.offset, 1);
    rc_gfx_encoder_draw(enc, &(rc_gfx_draw_desc) {.vertex_count = 3});
    rc_gfx_encoder_pass_end(enc);

    rc_gfx_encoder_pass_begin(enc, &(rc_gfx_pass_desc) {0});
    rc_gfx_encoder_set_pipeline(enc, blit_pip);
    rc_gfx_encoder_set_bind_group(enc, 0, blit_group, NULL, 0);
    rc_gfx_encoder_set_vertex_buffer(enc, 0, vbuf, 0);
    rc_gfx_encoder_draw(enc, &(rc_gfx_draw_desc) {.vertex_count = 3});
    rc_gfx_encoder_pass_end(enc);

    rc_gfx_cmd_buffer cb = rc_gfx_encoder_finish(enc);
    rc_gfx_submit(&cb, 1);
    rc_gfx_end_frame();

    // the far (green) draw failed the reverse-Z test: centre stays red
    uint32_t px = read_window_pixel(128, 128);
    RC_CHECK(channel_r(px), ==, 255);
    RC_CHECK(channel_g(px), ==, 0);

    rc_gfx_shutdown();
}

RC_TEST_STEP(gfx_gl, msaa_resolve, fix)
{
    // a 4x MSAA target cleared to solid red resolves (STORE_OP_RESOLVE)
    // into a single-sample texture, which is then sampled into the swapchain
    rc_gfx_init(&(rc_gfx_desc) {.arena = &fix->arena});

    rc_gfx_texture msaa = rc_gfx_texture_make(&(rc_gfx_texture_desc) {
        .format = RC_GFX_TEXTURE_FORMAT_RGBA8_UNORM,
        .size = {64, 64},
        .sample_count = 4,
        .usage = RC_GFX_TEXTURE_USAGE_RENDER_ATTACHMENT,
    });
    rc_gfx_texture resolve = rc_gfx_texture_make(&(rc_gfx_texture_desc) {
        .format = RC_GFX_TEXTURE_FORMAT_RGBA8_UNORM,
        .size = {64, 64},
        .usage = RC_GFX_TEXTURE_USAGE_RENDER_ATTACHMENT | RC_GFX_TEXTURE_USAGE_SAMPLED,
    });
    rc_gfx_render_target rt = rc_gfx_render_target_make(&(rc_gfx_render_target_desc) {
        .colors = {
            {
                .texture = msaa,
            },
        },
        .color_count = 1,
        .resolves = {
            {
                .texture = resolve,
            },
        },
    });

    static const char blit_vs[] =
        "layout(location = 0) in vec2 a_pos;\n"
        "out vec2 v_uv;\n"
        "void main() {\n"
        "    v_uv = a_pos * 0.5 + 0.5;\n"
        "    gl_Position = rc_clip(vec4(a_pos, 0.0, 1.0));\n"
        "}\n";
    static const char blit_fs[] =
        "uniform sampler2D u_tex;\n"
        "in vec2 v_uv;\n"
        "out vec4 o_color;\n"
        "void main() { o_color = texture(u_tex, v_uv); }\n";
    rc_gfx_shader shader = rc_gfx_shader_make(&(rc_gfx_shader_desc) {
        .vs_source = RC_STR(blit_vs),
        .fs_source = RC_STR(blit_fs),
        .texture_samplers = (const rc_gfx_shader_texture_sampler_pair[]) {
            {
                .glsl_name = RC_STR("u_tex"),
                .texture_binding = 0,
                .sampler_binding = 1,
            },
        },
        .texture_sampler_count = 1,
    });
    rc_gfx_sampler smp = rc_gfx_sampler_make(&(rc_gfx_sampler_desc) {0});
    rc_gfx_simple_layout layout = rc_gfx_simple_layout_make(
        (const rc_gfx_bind_group_layout_entry[]) {
            {.binding = 0, .visibility = RC_GFX_STAGE_FRAGMENT, .type = RC_GFX_BINDING_TEXTURE},
            {.binding = 1, .visibility = RC_GFX_STAGE_FRAGMENT, .type = RC_GFX_BINDING_SAMPLER},
        },
        2, RC_STR("resolve blit layout"));
    rc_gfx_bind_group group0 = rc_gfx_bind_group_make(&(rc_gfx_bind_group_desc) {
        .layout = layout.group0,
        .entries = {
            {.binding = 0, .texture = resolve},
            {.binding = 1, .sampler = smp},
        },
        .entry_count = 2,
    });
    rc_gfx_pipeline pip = rc_gfx_pipeline_make(&(rc_gfx_pipeline_desc) {
        .shader = shader,
        .layout = layout.layout,
        .vertex_layout = {
            .attributes = {
                {.location = 0, .format = RC_GFX_VERTEX_FORMAT_F32X2},
            },
        },
        .colors = {
            {
                .format = rc_gfx_swapchain_format(),
            },
        },
        .color_count = 1,
    });
    rc_gfx_buffer vbuf = make_fullscreen_vbuf();

    rc_gfx_begin_frame(rc_app_size());
    rc_gfx_encoder *enc = rc_gfx_encoder_begin(&fix->frame);

    rc_gfx_encoder_pass_begin(enc, &(rc_gfx_pass_desc) {
        .target = rt,
        .colors = {
            {
                .clear_value = {1.0f, 0.0f, 0.0f, 1.0f},
                .store_op = RC_GFX_STORE_OP_RESOLVE,
            },
        },
    });
    rc_gfx_encoder_pass_end(enc);

    rc_gfx_encoder_pass_begin(enc, &(rc_gfx_pass_desc) {0});
    rc_gfx_encoder_set_pipeline(enc, pip);
    rc_gfx_encoder_set_bind_group(enc, 0, group0, NULL, 0);
    rc_gfx_encoder_set_vertex_buffer(enc, 0, vbuf, 0);
    rc_gfx_encoder_draw(enc, &(rc_gfx_draw_desc) {.vertex_count = 3});
    rc_gfx_encoder_pass_end(enc);

    rc_gfx_cmd_buffer cb = rc_gfx_encoder_finish(enc);
    rc_gfx_submit(&cb, 1);
    rc_gfx_end_frame();

    uint32_t px = read_window_pixel(128, 128);
    RC_CHECK(channel_r(px), ==, 255);
    RC_CHECK(channel_g(px), ==, 0);

    rc_gfx_shutdown();
}

RC_TEST_STEP(gfx_gl, instanced_quads, fix)
{
    // per-instance attributes with a divisor: two instances of one quad at
    // different offsets and colours from a second, per-instance buffer
    rc_gfx_init(&(rc_gfx_desc) {.arena = &fix->arena});

    typedef struct instance {
        rc_vec2f offset;
        rc_vec4f color;
    } instance;
    static const float quad[12] = {
        -0.2f, -0.2f, 0.2f, -0.2f, 0.2f, 0.2f,
        -0.2f, -0.2f, 0.2f, 0.2f, -0.2f, 0.2f,
    };
    static const instance instances[2] = {
        {.offset = {-0.5f, 0.0f}, .color = {1.0f, 0.0f, 0.0f, 1.0f}},
        {.offset = {0.5f, 0.0f}, .color = {0.0f, 1.0f, 0.0f, 1.0f}},
    };
    static const char vs_src[] =
        "layout(location = 0) in vec2 a_pos;\n"
        "layout(location = 1) in vec2 a_offset;\n"
        "layout(location = 2) in vec4 a_color;\n"
        "out vec4 v_color;\n"
        "void main() {\n"
        "    v_color = a_color;\n"
        "    gl_Position = rc_clip(vec4(a_pos + a_offset, 0.0, 1.0));\n"
        "}\n";
    static const char fs_src[] =
        "in vec4 v_color;\n"
        "out vec4 o_color;\n"
        "void main() { o_color = v_color; }\n";

    rc_gfx_buffer vbuf = rc_gfx_buffer_make(&(rc_gfx_buffer_desc) {
        .size = sizeof(quad),
        .usage = RC_GFX_BUFFER_USAGE_VERTEX,
        .data = {.data = (const uint8_t *)quad, .num = sizeof(quad)},
    });
    rc_gfx_buffer ibuf = rc_gfx_buffer_make(&(rc_gfx_buffer_desc) {
        .size = sizeof(instances),
        .usage = RC_GFX_BUFFER_USAGE_VERTEX,
        .data = {.data = (const uint8_t *)instances, .num = sizeof(instances)},
    });
    rc_gfx_shader shader = rc_gfx_shader_make(&(rc_gfx_shader_desc) {
        .vs_source = RC_STR(vs_src),
        .fs_source = RC_STR(fs_src),
    });
    rc_gfx_pipeline_layout layout = rc_gfx_pipeline_layout_make(&(rc_gfx_pipeline_layout_desc) {0});
    rc_gfx_pipeline pip = rc_gfx_pipeline_make(&(rc_gfx_pipeline_desc) {
        .shader = shader,
        .layout = layout,
        .vertex_layout = {
            .buffers = {
                {0},
                {
                    .per_instance = true,
                },
            },
            .attributes = {
                {.location = 0, .buffer_index = 0, .format = RC_GFX_VERTEX_FORMAT_F32X2},
                {.location = 1, .buffer_index = 1, .format = RC_GFX_VERTEX_FORMAT_F32X2},
                {.location = 2, .buffer_index = 1, .format = RC_GFX_VERTEX_FORMAT_F32X4},
            },
        },
        .colors = {
            {
                .format = rc_gfx_swapchain_format(),
            },
        },
        .color_count = 1,
    });

    rc_vec2i size = rc_app_size();
    rc_gfx_begin_frame(size);
    rc_gfx_encoder *enc = rc_gfx_encoder_begin(&fix->frame);
    rc_gfx_encoder_pass_begin(enc, &(rc_gfx_pass_desc) {0});
    rc_gfx_encoder_set_pipeline(enc, pip);
    rc_gfx_encoder_set_vertex_buffer(enc, 0, vbuf, 0);
    rc_gfx_encoder_set_vertex_buffer(enc, 1, ibuf, 0);
    rc_gfx_encoder_draw(enc, &(rc_gfx_draw_desc) {
        .vertex_count = 6,
        .instance_count = 2,
    });
    rc_gfx_encoder_pass_end(enc);
    rc_gfx_cmd_buffer cb = rc_gfx_encoder_finish(enc);
    rc_gfx_submit(&cb, 1);
    rc_gfx_end_frame();

    // canonical NDC x = -0.5 -> window x = 0.25 * width, centred vertically
    uint32_t left = read_window_pixel(size.x / 4, size.y / 2);
    uint32_t right = read_window_pixel(3 * size.x / 4, size.y / 2);
    uint32_t centre = read_window_pixel(size.x / 2, size.y / 2);
    RC_CHECK(channel_r(left), ==, 255);
    RC_CHECK(channel_g(left), ==, 0);
    RC_CHECK(channel_r(right), ==, 0);
    RC_CHECK(channel_g(right), ==, 255);
    RC_CHECK(channel_r(centre), ==, 0);
    RC_CHECK(channel_g(centre), ==, 0);

    rc_gfx_shutdown();
}

RC_TEST_STEP(gfx_gl, storage_buffer_tbo, fix)
{
    // STORAGE_BUFFER_READ through the GL 3.3 TBO path: a float buffer read
    // with the portable RC_STORAGE_LOAD macro (texelFetch on a samplerBuffer)
    rc_gfx_init(&(rc_gfx_desc) {.arena = &fix->arena});

    static const float values[3] = {1.0f, 0.5f, 0.0f};
    rc_gfx_buffer sbuf = rc_gfx_buffer_make(&(rc_gfx_buffer_desc) {
        .size = sizeof(values),
        .usage = RC_GFX_BUFFER_USAGE_STORAGE,
        .data = {.data = (const uint8_t *)values, .num = sizeof(values)},
    });

    static const char vs_src[] =
        "layout(location = 0) in vec2 a_pos;\n"
        "void main() { gl_Position = rc_clip(vec4(a_pos, 0.0, 1.0)); }\n";
    static const char fs_src[] =
        "uniform samplerBuffer u_data;\n"
        "out vec4 o_color;\n"
        "void main() {\n"
        "    o_color = vec4(RC_STORAGE_LOAD(u_data, 0).r,\n"
        "                   RC_STORAGE_LOAD(u_data, 1).r,\n"
        "                   RC_STORAGE_LOAD(u_data, 2).r, 1.0);\n"
        "}\n";
    rc_gfx_shader shader = rc_gfx_shader_make(&(rc_gfx_shader_desc) {
        .vs_source = RC_STR(vs_src),
        .fs_source = RC_STR(fs_src),
        .texture_samplers = (const rc_gfx_shader_texture_sampler_pair[]) {
            {
                .glsl_name = RC_STR("u_data"),
                .texture_binding = 0,
            },
        },
        .texture_sampler_count = 1,
    });
    rc_gfx_simple_layout layout = rc_gfx_simple_layout_make(
        (const rc_gfx_bind_group_layout_entry[]) {
            {
                .binding = 0,
                .visibility = RC_GFX_STAGE_FRAGMENT,
                .type = RC_GFX_BINDING_STORAGE_BUFFER_READ,
                .texel_format = RC_GFX_TEXTURE_FORMAT_R32F,
            },
        },
        1, RC_STR("storage layout"));
    rc_gfx_bind_group group0 = rc_gfx_bind_group_make(&(rc_gfx_bind_group_desc) {
        .layout = layout.group0,
        .entries = {
            {.binding = 0, .buffer = sbuf},
        },
        .entry_count = 1,
    });
    rc_gfx_pipeline pip = rc_gfx_pipeline_make(&(rc_gfx_pipeline_desc) {
        .shader = shader,
        .layout = layout.layout,
        .vertex_layout = {
            .attributes = {
                {.location = 0, .format = RC_GFX_VERTEX_FORMAT_F32X2},
            },
        },
        .colors = {
            {
                .format = rc_gfx_swapchain_format(),
            },
        },
        .color_count = 1,
    });
    rc_gfx_buffer vbuf = make_fullscreen_vbuf();

    rc_gfx_begin_frame(rc_app_size());
    rc_gfx_encoder *enc = rc_gfx_encoder_begin(&fix->frame);
    rc_gfx_encoder_pass_begin(enc, &(rc_gfx_pass_desc) {0});
    rc_gfx_encoder_set_pipeline(enc, pip);
    rc_gfx_encoder_set_bind_group(enc, 0, group0, NULL, 0);
    rc_gfx_encoder_set_vertex_buffer(enc, 0, vbuf, 0);
    rc_gfx_encoder_draw(enc, &(rc_gfx_draw_desc) {.vertex_count = 3});
    rc_gfx_encoder_pass_end(enc);
    rc_gfx_cmd_buffer cb = rc_gfx_encoder_finish(enc);
    rc_gfx_submit(&cb, 1);
    rc_gfx_end_frame();

    // linear (1.0, 0.5, 0.0) presents as sRGB (255, 188, 0)
    uint32_t px = read_window_pixel(128, 128);
    RC_CHECK(channel_r(px), ==, 255);
    RC_CHECK_TRUE(near_u8(channel_g(px), 188, 3));
    RC_CHECK(channel_b(px), ==, 0);

    rc_gfx_shutdown();
}

RC_TEST_STEP(gfx_gl, texture_from_image, fix)
{
    // the rc_image bridge: an RGB8 image widens to RGBA on upload; a solid
    // orange texture drawn to the swapchain presents its sRGB bytes
    rc_gfx_init(&(rc_gfx_desc) {.arena = &fix->arena});

    // packed 0xAABBGGRR: sRGB orange (255, 128, 0)
    rc_image img = rc_image_make_filled(rc_vec2i_make(4, 4), RC_PIXEL_FORMAT_RGB8, 0xFF0080FFu, &fix->arena);
    rc_gfx_texture tex = rc_gfx_texture_from_image(img, true, false);

    static const char vs_src[] =
        "layout(location = 0) in vec2 a_pos;\n"
        "void main() { gl_Position = rc_clip(vec4(a_pos, 0.0, 1.0)); }\n";
    static const char fs_src[] =
        "uniform sampler2D u_tex;\n"
        "out vec4 o_color;\n"
        "void main() { o_color = texture(u_tex, vec2(0.5, 0.5)); }\n";
    rc_gfx_shader shader = rc_gfx_shader_make(&(rc_gfx_shader_desc) {
        .vs_source = RC_STR(vs_src),
        .fs_source = RC_STR(fs_src),
        .texture_samplers = (const rc_gfx_shader_texture_sampler_pair[]) {
            {
                .glsl_name = RC_STR("u_tex"),
                .texture_binding = 0,
                .sampler_binding = 1,
            },
        },
        .texture_sampler_count = 1,
    });
    rc_gfx_sampler smp = rc_gfx_sampler_make(&(rc_gfx_sampler_desc) {0});
    rc_gfx_simple_layout layout = rc_gfx_simple_layout_make(
        (const rc_gfx_bind_group_layout_entry[]) {
            {.binding = 0, .visibility = RC_GFX_STAGE_FRAGMENT, .type = RC_GFX_BINDING_TEXTURE},
            {.binding = 1, .visibility = RC_GFX_STAGE_FRAGMENT, .type = RC_GFX_BINDING_SAMPLER},
        },
        2, RC_STR("image layout"));
    rc_gfx_bind_group group0 = rc_gfx_bind_group_make(&(rc_gfx_bind_group_desc) {
        .layout = layout.group0,
        .entries = {
            {.binding = 0, .texture = tex},
            {.binding = 1, .sampler = smp},
        },
        .entry_count = 2,
    });
    rc_gfx_pipeline pip = rc_gfx_pipeline_make(&(rc_gfx_pipeline_desc) {
        .shader = shader,
        .layout = layout.layout,
        .vertex_layout = {
            .attributes = {
                {.location = 0, .format = RC_GFX_VERTEX_FORMAT_F32X2},
            },
        },
        .colors = {
            {
                .format = rc_gfx_swapchain_format(),
            },
        },
        .color_count = 1,
    });
    rc_gfx_buffer vbuf = make_fullscreen_vbuf();

    rc_gfx_begin_frame(rc_app_size());
    rc_gfx_encoder *enc = rc_gfx_encoder_begin(&fix->frame);
    rc_gfx_encoder_pass_begin(enc, &(rc_gfx_pass_desc) {0});
    rc_gfx_encoder_set_pipeline(enc, pip);
    rc_gfx_encoder_set_bind_group(enc, 0, group0, NULL, 0);
    rc_gfx_encoder_set_vertex_buffer(enc, 0, vbuf, 0);
    rc_gfx_encoder_draw(enc, &(rc_gfx_draw_desc) {.vertex_count = 3});
    rc_gfx_encoder_pass_end(enc);
    rc_gfx_cmd_buffer cb = rc_gfx_encoder_finish(enc);
    rc_gfx_submit(&cb, 1);
    rc_gfx_end_frame();

    // sRGB texel decoded to linear, re-encoded on present: bytes round-trip
    uint32_t px = read_window_pixel(128, 128);
    RC_CHECK_TRUE(near_u8(channel_r(px), 255, 1));
    RC_CHECK_TRUE(near_u8(channel_g(px), 128, 2));
    RC_CHECK_TRUE(near_u8(channel_b(px), 0, 1));

    rc_gfx_shutdown();
}

RC_TEST_STEP(gfx_gl, swapchain_resize, fix)
{
    // window resizes recreate the swapchain target between frames; GL reuses
    // freed object names, so the state shadow must not treat the recreated
    // texture as already bound (this trapped with an incomplete FBO once)
    rc_gfx_init(&(rc_gfx_desc) {.arena = &fix->arena});
    run_clear_frame(&fix->frame, rc_vec4f_make(1.0f, 0.0f, 0.0f, 1.0f));

    // shrink: the present draw covers the bottom-left of the window in GL
    // coordinates, so read there rather than through the window-sized helper
    rc_gfx_begin_frame(rc_vec2i_make(128, 128));
    rc_arena_reset(&fix->frame);
    rc_gfx_encoder *enc = rc_gfx_encoder_begin(&fix->frame);
    rc_gfx_encoder_pass_begin(enc, &(rc_gfx_pass_desc) {
        .colors = {
            {
                .clear_value = {0.0f, 1.0f, 0.0f, 1.0f},
            },
        },
    });
    rc_gfx_encoder_pass_end(enc);
    rc_gfx_cmd_buffer cb = rc_gfx_encoder_finish(enc);
    rc_gfx_submit(&cb, 1);
    rc_gfx_end_frame();

    uint8_t px[4] = {0};
    glReadPixels(10, 10, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
    RC_CHECK(px[0], ==, 0);
    RC_CHECK(px[1], ==, 255);

    // grow again, back through recreation a second time
    run_clear_frame(&fix->frame, rc_vec4f_make(0.0f, 0.0f, 1.0f, 1.0f));
    uint32_t full = read_window_pixel(128, 128);
    RC_CHECK(channel_b(full), ==, 255);

    rc_gfx_shutdown();
}
