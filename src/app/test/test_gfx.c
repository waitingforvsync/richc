/*
 * test_gfx.c - device-less gfx tests: format tables, sRGB conversion
 * helpers, mip counts, handle encoding, and command encoding/dump.
 *
 * Everything here runs without a GL context; the tests that need one live in
 * test_gfx_gl.c.
 */

#include "richc/arena.h"
#include "richc/gfx/color.h"
#include "richc/gfx/encoder.h"
#include "richc/gfx/gfx.h"
#include "richc/gfx/pipeline.h"
#include "richc/gfx/texture.h"
#include "richc/test.h"

RC_TEST(gfx_format, block_size)
{
    RC_CHECK(rc_gfx_texture_format_block_size(RC_GFX_TEXTURE_FORMAT_NONE), ==, 0u);
    RC_CHECK(rc_gfx_texture_format_block_size(RC_GFX_TEXTURE_FORMAT_R8_UNORM), ==, 1u);
    RC_CHECK(rc_gfx_texture_format_block_size(RC_GFX_TEXTURE_FORMAT_RG8_UNORM), ==, 2u);
    RC_CHECK(rc_gfx_texture_format_block_size(RC_GFX_TEXTURE_FORMAT_RGBA8_SRGB), ==, 4u);
    RC_CHECK(rc_gfx_texture_format_block_size(RC_GFX_TEXTURE_FORMAT_RGBA16F), ==, 8u);
    RC_CHECK(rc_gfx_texture_format_block_size(RC_GFX_TEXTURE_FORMAT_RGBA32F), ==, 16u);
    RC_CHECK(rc_gfx_texture_format_block_size(RC_GFX_TEXTURE_FORMAT_RG11B10F), ==, 4u);
    RC_CHECK(rc_gfx_texture_format_block_size(RC_GFX_TEXTURE_FORMAT_DEPTH32F_STENCIL8), ==, 8u);
    // BC blocks are bytes per 4x4 block
    RC_CHECK(rc_gfx_texture_format_block_size(RC_GFX_TEXTURE_FORMAT_BC1_RGBA_UNORM), ==, 8u);
    RC_CHECK(rc_gfx_texture_format_block_size(RC_GFX_TEXTURE_FORMAT_BC7_RGBA_UNORM), ==, 16u);
}

RC_TEST(gfx_format, block_dim)
{
    RC_CHECK(rc_gfx_texture_format_block_dim(RC_GFX_TEXTURE_FORMAT_RGBA8_UNORM), ==, rc_vec2i_make(1, 1));
    RC_CHECK(rc_gfx_texture_format_block_dim(RC_GFX_TEXTURE_FORMAT_BC5_RG_UNORM), ==, rc_vec2i_make(4, 4));
}

RC_TEST(gfx_format, properties)
{
    RC_CHECK_TRUE(rc_gfx_texture_format_is_srgb(RC_GFX_TEXTURE_FORMAT_RGBA8_SRGB));
    RC_CHECK_FALSE(rc_gfx_texture_format_is_srgb(RC_GFX_TEXTURE_FORMAT_RGBA8_UNORM));
    RC_CHECK_TRUE(rc_gfx_texture_format_is_depth(RC_GFX_TEXTURE_FORMAT_DEPTH24_PLUS));
    RC_CHECK_FALSE(rc_gfx_texture_format_is_depth(RC_GFX_TEXTURE_FORMAT_R32F));
    RC_CHECK_TRUE(rc_gfx_texture_format_is_stencil(RC_GFX_TEXTURE_FORMAT_DEPTH24_PLUS_STENCIL8));
    RC_CHECK_FALSE(rc_gfx_texture_format_is_stencil(RC_GFX_TEXTURE_FORMAT_DEPTH32F));
    RC_CHECK_TRUE(rc_gfx_texture_format_is_compressed(RC_GFX_TEXTURE_FORMAT_BC6H_RGB_FLOAT));
    RC_CHECK_FALSE(rc_gfx_texture_format_is_compressed(RC_GFX_TEXTURE_FORMAT_RGBA8_UNORM));
}

RC_TEST(gfx_format, srgb_pairs)
{
    // every sRGB format maps to its linear twin and back
    rc_gfx_texture_format srgb_formats[] = {
        RC_GFX_TEXTURE_FORMAT_RGBA8_SRGB, RC_GFX_TEXTURE_FORMAT_BGRA8_SRGB,
        RC_GFX_TEXTURE_FORMAT_BC1_RGBA_SRGB, RC_GFX_TEXTURE_FORMAT_BC3_RGBA_SRGB,
        RC_GFX_TEXTURE_FORMAT_BC7_RGBA_SRGB,
    };
    for (uint32_t i = 0; i < sizeof(srgb_formats) / sizeof(srgb_formats[0]); i += 1) {
        rc_gfx_texture_format srgb = srgb_formats[i];
        rc_gfx_texture_format linear = rc_gfx_texture_format_to_linear(srgb);
        RC_CHECK_FALSE(rc_gfx_texture_format_is_srgb(linear));
        RC_CHECK((uint32_t)rc_gfx_texture_format_to_srgb(linear), ==, (uint32_t)srgb);
    }
    // formats with no counterpart are identity in both directions
    RC_CHECK((uint32_t)rc_gfx_texture_format_to_srgb(RC_GFX_TEXTURE_FORMAT_R8_UNORM), ==,
             (uint32_t)RC_GFX_TEXTURE_FORMAT_R8_UNORM);
    RC_CHECK((uint32_t)rc_gfx_texture_format_to_linear(RC_GFX_TEXTURE_FORMAT_RGBA16F), ==,
             (uint32_t)RC_GFX_TEXTURE_FORMAT_RGBA16F);
}

RC_TEST(gfx_color, srgb_curve)
{
    // endpoints are exact
    RC_CHECK(rc_gfx_srgb_to_linear(0.0f), ==, 0.0f);
    RC_CHECK(rc_gfx_srgb_to_linear(1.0f), ~=, 1.0f);
    RC_CHECK(rc_gfx_linear_to_srgb(0.0f), ==, 0.0f);
    RC_CHECK(rc_gfx_linear_to_srgb(1.0f), ~=, 1.0f);
    // the canonical mid-grey checks: 0.5 linear encodes to ~0.7354 (188/255)
    RC_CHECK(rc_gfx_linear_to_srgb(0.5f), ~=, 0.73536f);
    RC_CHECK(rc_gfx_srgb_to_linear(0.5f), ~=, 0.21404f);
    // the linear segment near black must not use the power curve
    RC_CHECK(rc_gfx_srgb_to_linear(0.02f), ~=, 0.02f / 12.92f);
    RC_CHECK(rc_gfx_linear_to_srgb(0.001f), ~=, 0.001f * 12.92f);
    // round trip across the range
    for (uint32_t i = 0; i <= 20; i += 1) {
        float s = (float)i / 20.0f;
        float diff = rc_gfx_linear_to_srgb(rc_gfx_srgb_to_linear(s)) - s;
        RC_CHECK_TRUE(diff < 1e-5f && diff > -1e-5f);
    }
}

RC_TEST(gfx_color, packed)
{
    // 0xAABBGGRR: R low byte; alpha passes through linearly
    rc_vec4f mid = rc_gfx_color_from_srgb_u32(0x80BC8080u);
    RC_CHECK(mid.x, ~=, 0.21586f);   // R = 0x80 = 128 -> ~0.216 linear
    RC_CHECK(mid.y, ~=, 0.21586f);   // G = 0x80
    RC_CHECK(mid.z, ~=, 0.50289f);   // B = 0xBC = 188 -> ~0.5 linear
    RC_CHECK(mid.w, ~=, 128.0f / 255.0f);
    RC_CHECK(rc_gfx_color_to_srgb_u32(mid), ==, 0x80BC8080u);
    // extremes survive the round trip, and out-of-range input clamps
    RC_CHECK(rc_gfx_color_to_srgb_u32(rc_gfx_color_from_srgb_u32(0xFFFFFFFFu)), ==, 0xFFFFFFFFu);
    RC_CHECK(rc_gfx_color_to_srgb_u32(rc_gfx_color_from_srgb_u32(0x00000000u)), ==, 0x00000000u);
    RC_CHECK(rc_gfx_color_to_srgb_u32(rc_vec4f_make(2.0f, -1.0f, 0.0f, 3.0f)), ==, 0xFF0000FFu);
}

RC_TEST(gfx_misc, mip_count)
{
    RC_CHECK(rc_gfx_mip_count(rc_vec2i_make(1, 1)), ==, 1u);
    RC_CHECK(rc_gfx_mip_count(rc_vec2i_make(2, 2)), ==, 2u);
    RC_CHECK(rc_gfx_mip_count(rc_vec2i_make(2, 1)), ==, 2u);
    RC_CHECK(rc_gfx_mip_count(rc_vec2i_make(256, 256)), ==, 9u);
    RC_CHECK(rc_gfx_mip_count(rc_vec2i_make(640, 480)), ==, 10u);
    RC_CHECK(rc_gfx_mip_count(rc_vec2i_make(1, 1024)), ==, 11u);
}

RC_TEST(gfx_misc, handle_functions)
{
    rc_gfx_buffer none = {0};
    RC_CHECK_TRUE(rc_genpool_handle_is_null(none.h));
    rc_gfx_buffer buf = {.h = rc_genpool_handle_make(4, 7)};
    RC_CHECK_FALSE(rc_genpool_handle_is_null(buf.h));
    RC_CHECK(rc_genpool_handle_index(buf.h), ==, 4u);
    RC_CHECK(rc_genpool_handle_gen(buf.h), ==, 7u);
    RC_CHECK_TRUE(rc_genpool_handle_equal(buf.h, rc_genpool_handle_make(4, 7)));
    RC_CHECK_FALSE(rc_genpool_handle_equal(buf.h, none.h));
}

RC_TEST(gfx_misc, blend_state_helpers)
{
    rc_gfx_blend_state alpha = rc_gfx_blend_state_make_alpha();
    RC_CHECK_TRUE(alpha.enabled);
    RC_CHECK((uint32_t)alpha.src_factor, ==, (uint32_t)RC_GFX_BLEND_FACTOR_SRC_ALPHA);
    RC_CHECK((uint32_t)alpha.dst_factor, ==, (uint32_t)RC_GFX_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA);
    rc_gfx_blend_state pre = rc_gfx_blend_state_make_premultiplied();
    RC_CHECK((uint32_t)pre.src_factor, ==, (uint32_t)RC_GFX_BLEND_FACTOR_ONE);
    rc_gfx_blend_state add = rc_gfx_blend_state_make_additive();
    RC_CHECK((uint32_t)add.dst_factor, ==, (uint32_t)RC_GFX_BLEND_FACTOR_ONE);
    RC_CHECK((uint32_t)add.op, ==, (uint32_t)RC_GFX_BLEND_OP_ADD);
}

RC_TEST(gfx_encoder, record_and_dump)
{
    rc_arena arena = rc_arena_make_default();
    rc_gfx_encoder *enc = rc_gfx_encoder_begin(&arena);

    // record one of everything (handles are fabricated - encoding never
    // resolves them, only submit does)
    rc_gfx_encoder_push_debug_group(enc, RC_STR("frame"));
    rc_gfx_encoder_pass_begin(enc, &(rc_gfx_pass_desc) {
        .colors = {
            {
                .clear_value = {0.25f, 0.5f, 0.75f, 1.0f},
            },
        },
        .label = RC_STR("main"),
    });
    rc_gfx_encoder_set_pipeline(enc, (rc_gfx_pipeline) {.h = rc_genpool_handle_make(0, 0)});
    uint32_t offsets[2] = {256, 512};
    rc_gfx_encoder_set_bind_group(enc, 0, (rc_gfx_bind_group) {.h = rc_genpool_handle_make(1, 0)}, offsets, 2);
    rc_gfx_encoder_set_vertex_buffer(enc, 1, (rc_gfx_buffer) {.h = rc_genpool_handle_make(2, 0)}, 64);
    rc_gfx_encoder_set_index_buffer(enc, (rc_gfx_buffer) {.h = rc_genpool_handle_make(3, 0)}, RC_GFX_INDEX_FORMAT_U16, 128);
    rc_gfx_encoder_set_viewport(enc, rc_box2i_make_pos_size(rc_vec2i_make(0, 0), rc_vec2i_make(64, 32)), 0.0f, 1.0f);
    rc_gfx_encoder_set_scissor(enc, rc_box2i_make_pos_size(rc_vec2i_make(8, 8), rc_vec2i_make(16, 16)));
    rc_gfx_encoder_set_blend_constant(enc, rc_vec4f_make(1.0f, 0.5f, 0.25f, 1.0f));
    rc_gfx_encoder_set_stencil_reference(enc, 3);
    rc_gfx_encoder_draw(enc, &(rc_gfx_draw_desc) {.vertex_count = 3});
    rc_gfx_encoder_draw_indexed(enc, &(rc_gfx_draw_indexed_desc) {
        .index_count = 6,
        .instance_count = 4,
        .first_index = 6,
        .base_vertex = -2,
    });
    rc_gfx_encoder_pass_end(enc);
    rc_gfx_encoder_pop_debug_group(enc);

    rc_gfx_cmd_buffer cb = rc_gfx_encoder_finish(enc);
    RC_CHECK_TRUE(cb.size > 0);

    rc_mstr dump = rc_gfx_cmd_buffer_dump(cb, &arena);
    rc_str text = dump.view;
    RC_CHECK_TRUE(rc_str_contains(text, RC_STR("push_debug_group \"frame\"")));
    RC_CHECK_TRUE(rc_str_contains(text, RC_STR("pass_begin target=none")));
    RC_CHECK_TRUE(rc_str_contains(text, RC_STR("label=\"main\"")));
    RC_CHECK_TRUE(rc_str_contains(text, RC_STR("set_pipeline id=0:0")));
    RC_CHECK_TRUE(rc_str_contains(text, RC_STR("set_bind_group group=0 id=1:0 offsets=[256,512]")));
    RC_CHECK_TRUE(rc_str_contains(text, RC_STR("set_vertex_buffer slot=1 id=2:0 offset=64")));
    RC_CHECK_TRUE(rc_str_contains(text, RC_STR("set_index_buffer id=3:0 fmt=u16 offset=128")));
    RC_CHECK_TRUE(rc_str_contains(text, RC_STR("set_viewport rect=[0,0 64x32]")));
    RC_CHECK_TRUE(rc_str_contains(text, RC_STR("set_scissor rect=[8,8 16x16]")));
    RC_CHECK_TRUE(rc_str_contains(text, RC_STR("set_stencil_reference ref=3")));
    RC_CHECK_TRUE(rc_str_contains(text, RC_STR("draw vertices=3 instances=1 first_vertex=0")));
    RC_CHECK_TRUE(rc_str_contains(text, RC_STR("draw_indexed indices=6 instances=4 first_index=6 base_vertex=-2")));
    RC_CHECK_TRUE(rc_str_contains(text, RC_STR("pass_end")));
    RC_CHECK_TRUE(rc_str_contains(text, RC_STR("pop_debug_group")));

    rc_arena_deinit(&arena);
}

RC_TEST(gfx_encoder, empty_finish)
{
    rc_arena arena = rc_arena_make_default();
    rc_gfx_encoder *enc = rc_gfx_encoder_begin(&arena);
    rc_gfx_cmd_buffer cb = rc_gfx_encoder_finish(enc);
    RC_CHECK(cb.size, ==, 0u);
    rc_mstr dump = rc_gfx_cmd_buffer_dump(cb, &arena);
    RC_CHECK_TRUE(!rc_mstr_is_valid(&dump) || dump.len == 0);
    rc_arena_deinit(&arena);
}
