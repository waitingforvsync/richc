#include "richc/arena.h"
#include "richc/image/image_atlas.h"
#include "richc/math/box2i.h"
#include "richc/test.h"

RC_TEST_GROUP_DATA(image_atlas) {
    rc_arena a;   // a single arena - the incremental atlas needs no scratch
};

RC_TEST_GROUP_INIT(image_atlas, fix)
{
    fix->a = rc_arena_make_default();
}

RC_TEST_GROUP_DEINIT(image_atlas, fix)
{
    rc_arena_deinit(&fix->a);
}

/* ---- helpers ---- */

static rc_image solid(rc_arena *a, int32_t w, int32_t h, rc_pixel_format fmt, uint32_t fill)
{
    return rc_image_make_filled(rc_vec2i_make(w, h), fmt, fill, a);
}

static uint32_t sample_center(rc_image atlas, rc_vec2i pos, rc_vec2i sz)
{
    return rc_image_get_pixel(atlas, pos.x + sz.x / 2, pos.y + sz.y / 2);
}

// True if a placed image (pos+size), inflated by spacing on all sides, overlaps
// another placed image. Touching edges do not count (half-open intersects).
static bool overlaps(rc_vec2i pa, rc_vec2i sa, rc_vec2i pb, rc_vec2i sb, int32_t spacing)
{
    rc_box2i a = rc_box2i_make_with_margin(pa, rc_vec2i_make(pa.x + sa.x, pa.y + sa.y), spacing);
    rc_box2i b = rc_box2i_make_pos_size(pb, sb);
    return rc_box2i_intersects(a, b);
}

/* ---- make ---- */

RC_TEST_STEP(image_atlas, make_cleared, fix)
{
    rc_image_atlas atlas = rc_image_atlas_make(rc_vec2i_make(64, 64),
                                               RC_PIXEL_FORMAT_RGBA8, 1, &fix->a);
    RC_CHECK_TRUE(atlas.image.data.data != NULL);
    RC_CHECK(atlas.image.size.x, ==, 64);
    RC_CHECK(atlas.image.size.y, ==, 64);
    RC_CHECK(atlas.image.format, ==, RC_PIXEL_FORMAT_RGBA8);
    // cleared: any pixel reads zero before anything is added
    RC_CHECK(rc_image_get_pixel(atlas.image, 10, 10), ==, 0x00000000u);
}

/* ---- add ---- */

RC_TEST_STEP(image_atlas, add_single, fix)
{
    rc_image_atlas atlas = rc_image_atlas_make(rc_vec2i_make(64, 64),
                                               RC_PIXEL_FORMAT_RGBA8, 0, &fix->a);
    rc_image src = solid(&fix->a, 10, 20, RC_PIXEL_FORMAT_RGBA8, 0x80112233u);

    rc_rect_pack_result r = rc_image_atlas_add(&atlas, src, &fix->a);
    RC_CHECK_TRUE(r.placed);
    RC_CHECK(r.pos.x, ==, 0);
    RC_CHECK(r.pos.y, ==, 0);
    RC_CHECK(sample_center(atlas.image, r.pos, src.size), ==, 0x80112233u);
}

RC_TEST_STEP(image_atlas, add_multiple, fix)
{
    rc_image_atlas atlas = rc_image_atlas_make(rc_vec2i_make(64, 64),
                                               RC_PIXEL_FORMAT_RGBA8, 2, &fix->a);
    rc_image src[] = {
        solid(&fix->a, 20, 20, RC_PIXEL_FORMAT_RGBA8, 0xFF0000FFu),
        solid(&fix->a, 16, 24, RC_PIXEL_FORMAT_RGBA8, 0xFF00FF00u),
        solid(&fix->a, 24, 16, RC_PIXEL_FORMAT_RGBA8, 0xFFFF0000u),
    };
    rc_vec2i pos[3];
    for (uint32_t i = 0; i < 3; i++) {
        rc_rect_pack_result r = rc_image_atlas_add(&atlas, src[i], &fix->a);
        RC_CHECK_TRUE(r.placed);
        pos[i] = r.pos;
    }

    // non-overlapping (with the 2px gap) and each region holds its own colour
    for (uint32_t i = 0; i < 3; i++) {
        RC_CHECK(sample_center(atlas.image, pos[i], src[i].size),
                 ==, rc_image_get_pixel(src[i], src[i].size.x / 2, src[i].size.y / 2));
        for (uint32_t j = i + 1; j < 3; j++)
            RC_CHECK(overlaps(pos[i], src[i].size, pos[j], src[j].size, 2), ==, false);
    }
}

RC_TEST_STEP(image_atlas, placements_stable, fix)
{
    // an early placement must not move, nor its pixels change, as more are added
    rc_image_atlas atlas = rc_image_atlas_make(rc_vec2i_make(128, 128),
                                               RC_PIXEL_FORMAT_RGBA8, 1, &fix->a);
    rc_image first = solid(&fix->a, 20, 20, RC_PIXEL_FORMAT_RGBA8, 0xFF123456u);
    rc_rect_pack_result r0 = rc_image_atlas_add(&atlas, first, &fix->a);
    RC_CHECK_TRUE(r0.placed);

    // add several more (forcing the free list to grow)
    for (uint32_t i = 0; i < 8; i++) {
        rc_image more = solid(&fix->a, 15, 15, RC_PIXEL_FORMAT_RGBA8, 0xFF808080u);
        rc_rect_pack_result r = rc_image_atlas_add(&atlas, more, &fix->a);
        RC_CHECK_TRUE(r.placed);
        // the first placement's position is unchanged...
        RC_CHECK(r.pos.x == r0.pos.x && r.pos.y == r0.pos.y, ==, false);
    }
    // ...and its pixels are still intact
    RC_CHECK(sample_center(atlas.image, r0.pos, first.size), ==, 0xFF123456u);
}

RC_TEST_STEP(image_atlas, fill_until_full, fix)
{
    // 32x32 cells in 64x64 with no spacing: exactly four fit, the fifth fails
    rc_image_atlas atlas = rc_image_atlas_make(rc_vec2i_make(64, 64),
                                               RC_PIXEL_FORMAT_RGBA8, 0, &fix->a);
    rc_vec2i first_pos = {0};
    for (int i = 0; i < 4; i++) {
        rc_image cell = solid(&fix->a, 32, 32, RC_PIXEL_FORMAT_RGBA8, 0xFF00AAFFu);
        rc_rect_pack_result r = rc_image_atlas_add(&atlas, cell, &fix->a);
        RC_CHECK_TRUE(r.placed);
        if (i == 0) first_pos = r.pos;
    }
    rc_image overflow = solid(&fix->a, 32, 32, RC_PIXEL_FORMAT_RGBA8, 0xFFFFFFFFu);
    rc_rect_pack_result r = rc_image_atlas_add(&atlas, overflow, &fix->a);
    RC_CHECK_FALSE(r.placed);
    // the failed add left earlier content intact
    RC_CHECK(sample_center(atlas.image, first_pos, rc_vec2i_make(32, 32)), ==, 0xFF00AAFFu);
}

RC_TEST_STEP(image_atlas, format_widening, fix)
{
    // an R8 source widens into an RGBA8 atlas: 0xAA -> 0xFFAAAAAA
    rc_image_atlas atlas = rc_image_atlas_make(rc_vec2i_make(64, 64),
                                               RC_PIXEL_FORMAT_RGBA8, 0, &fix->a);
    rc_image src = solid(&fix->a, 16, 16, RC_PIXEL_FORMAT_R8, 0xAAu);
    rc_rect_pack_result r = rc_image_atlas_add(&atlas, src, &fix->a);
    RC_CHECK_TRUE(r.placed);
    RC_CHECK(sample_center(atlas.image, r.pos, src.size), ==, 0xFFAAAAAAu);
}

RC_TEST_STEP(image_atlas, gaps_cleared, fix)
{
    rc_image_atlas atlas = rc_image_atlas_make(rc_vec2i_make(64, 64),
                                               RC_PIXEL_FORMAT_RGBA8, 0, &fix->a);
    rc_image src = solid(&fix->a, 8, 8, RC_PIXEL_FORMAT_RGBA8, 0xFFFFFFFFu);
    rc_rect_pack_result r = rc_image_atlas_add(&atlas, src, &fix->a);
    RC_CHECK_TRUE(r.placed);
    // a pixel far from the 8x8 placement at (0,0) stays cleared
    RC_CHECK(rc_image_get_pixel(atlas.image, 60, 60), ==, 0x00000000u);
}
