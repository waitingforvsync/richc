#include "richc/arena.h"
#include "richc/image/image_pack.h"
#include "richc/math/box2i.h"
#include "richc/test.h"

RC_TEST_GROUP_DATA(image_pack) {
    rc_arena a;
    rc_arena scratch;
};

RC_TEST_GROUP_INIT(image_pack, fix)
{
    fix->a       = rc_arena_make_default();
    fix->scratch = rc_arena_make_default();
}

RC_TEST_GROUP_DEINIT(image_pack, fix)
{
    rc_arena_deinit(&fix->a);
    rc_arena_deinit(&fix->scratch);
}

/* ---- helpers ---- */

// A solid-colour image of the given size/format, filled with the packed colour.
static rc_image solid(rc_arena *a, int32_t w, int32_t h, rc_pixel_format fmt, uint32_t fill)
{
    return rc_image_make_filled(rc_vec2i_make(w, h), fmt, fill, a);
}

// The atlas pixel at the centre of the region where images[i] was placed.
static uint32_t sample_center(rc_image atlas, rc_vec2i pos, rc_vec2i sz)
{
    return rc_image_get_pixel(atlas, pos.x + sz.x / 2, pos.y + sz.y / 2);
}

// True if a placed image (pos+size), inflated by spacing on all sides, overlaps
// another placed image.  Touching edges do not count (half-open intersects).
static bool overlaps(rc_vec2i pa, rc_vec2i sa, rc_vec2i pb, rc_vec2i sb, int32_t spacing)
{
    rc_box2i a = rc_box2i_make_with_margin(pa, rc_vec2i_make(pa.x + sa.x, pa.y + sa.y), spacing);
    rc_box2i b = rc_box2i_make_pos_size(pb, sb);
    return rc_box2i_intersects(a, b);
}

// Assert every placement is in-bounds and no pair violates the spacing gap.
static void check_atlas(rc_view_image imgs, rc_span_vec2i pos,
                        rc_vec2i container, int32_t spacing)
{
    for (uint32_t i = 0; i < imgs.num; i++) {
        rc_vec2i p = rc_span_vec2i_get(pos, i);
        rc_vec2i s = rc_view_image_get(imgs, i).size;
        RC_CHECK(p.x >= 0, ==, true);
        RC_CHECK(p.y >= 0, ==, true);
        RC_CHECK(p.x + s.x <= container.x, ==, true);
        RC_CHECK(p.y + s.y <= container.y, ==, true);
        for (uint32_t j = i + 1; j < imgs.num; j++)
            RC_CHECK(overlaps(p, s, rc_span_vec2i_get(pos, j),
                              rc_view_image_get(imgs, j).size, spacing), ==, false);
    }
}

/* ---- edge cases ---- */

RC_TEST_STEP(image_pack, empty, fix)
{
    rc_image_pack_result r = rc_image_pack((rc_view_image) {0}, rc_vec2i_make(64, 64),
                                           0, &fix->a, fix->scratch);
    RC_CHECK_TRUE(r.image.data.data == NULL);
    RC_CHECK(r.positions.num, ==, 0u);
}

RC_TEST_STEP(image_pack, single, fix)
{
    rc_image imgs[] = {solid(&fix->a, 10, 20, RC_PIXEL_FORMAT_RGBA8, 0x80112233u)};
    rc_view_image v = RC_VIEW(imgs);
    rc_vec2i atlas_size = rc_vec2i_make(64, 64);

    rc_image_pack_result r = rc_image_pack(v, atlas_size, 0, &fix->a, fix->scratch);
    RC_CHECK_TRUE(r.image.data.data != NULL);
    RC_CHECK(r.image.format, ==, RC_PIXEL_FORMAT_RGBA8);
    RC_CHECK(r.image.size.x, ==, 64);
    RC_CHECK(r.image.size.y, ==, 64);
    RC_CHECK(r.positions.num, ==, 1u);

    rc_vec2i p0 = rc_span_vec2i_get(r.positions, 0);
    RC_CHECK(p0.x, ==, 0);
    RC_CHECK(p0.y, ==, 0);
    RC_CHECK(sample_center(r.image, p0, imgs[0].size), ==, 0x80112233u);
}

RC_TEST_STEP(image_pack, overflow_fixed, fix)
{
    // an image larger than the fixed atlas fails the whole pack
    rc_image imgs[] = {solid(&fix->a, 100, 10, RC_PIXEL_FORMAT_R8, 0x55u)};
    rc_view_image v = RC_VIEW(imgs);
    rc_image_pack_result r = rc_image_pack(v, rc_vec2i_make(64, 64), 0, &fix->a, fix->scratch);
    RC_CHECK_TRUE(r.image.data.data == NULL);
    RC_CHECK(r.positions.num, ==, 0u);
}

/* ---- multiple images ---- */

RC_TEST_STEP(image_pack, multiple_same_format, fix)
{
    rc_image imgs[] = {
        solid(&fix->a, 20, 20, RC_PIXEL_FORMAT_RGBA8, 0xFF0000FFu),
        solid(&fix->a, 16, 24, RC_PIXEL_FORMAT_RGBA8, 0xFF00FF00u),
        solid(&fix->a, 24, 16, RC_PIXEL_FORMAT_RGBA8, 0xFFFF0000u),
    };
    rc_view_image v = RC_VIEW(imgs);
    rc_vec2i container = rc_vec2i_make(64, 64);

    rc_image_pack_result r = rc_image_pack(v, container, 2, &fix->a, fix->scratch);
    RC_CHECK_TRUE(r.image.data.data != NULL);
    RC_CHECK(r.positions.num, ==, 3u);
    check_atlas(v, r.positions, container, 2);

    // each placed region holds its own colour
    for (uint32_t i = 0; i < 3; i++)
        RC_CHECK(sample_center(r.image, rc_span_vec2i_get(r.positions, i), imgs[i].size),
                 ==, rc_image_get_pixel(imgs[i], imgs[i].size.x / 2, imgs[i].size.y / 2));
}

RC_TEST_STEP(image_pack, mixed_formats_widen, fix)
{
    // widest format wins: R8 + RGB8 + RGBA8 -> RGBA8 atlas; sources widen in
    rc_image imgs[] = {
        solid(&fix->a, 16, 16, RC_PIXEL_FORMAT_R8,    0xAAu),        // -> 0xFFAAAAAA
        solid(&fix->a, 16, 16, RC_PIXEL_FORMAT_RGB8,  0x00112233u),  // -> 0xFF112233
        solid(&fix->a, 16, 16, RC_PIXEL_FORMAT_RGBA8, 0x80112233u),  // -> 0x80112233
    };
    rc_view_image v = RC_VIEW(imgs);
    rc_vec2i container = rc_vec2i_make(64, 64);

    rc_image_pack_result r = rc_image_pack(v, container, 1, &fix->a, fix->scratch);
    RC_CHECK_TRUE(r.image.data.data != NULL);
    RC_CHECK(r.image.format, ==, RC_PIXEL_FORMAT_RGBA8);
    check_atlas(v, r.positions, container, 1);

    RC_CHECK(sample_center(r.image, rc_span_vec2i_get(r.positions, 0), imgs[0].size), ==, 0xFFAAAAAAu);
    RC_CHECK(sample_center(r.image, rc_span_vec2i_get(r.positions, 1), imgs[1].size), ==, 0xFF112233u);
    RC_CHECK(sample_center(r.image, rc_span_vec2i_get(r.positions, 2), imgs[2].size), ==, 0x80112233u);
}

RC_TEST_STEP(image_pack, input_order_indexing, fix)
{
    // mixed sizes force the internal decreasing-max-side sort; positions must
    // still be indexed by input order (region i holds image i's colour)
    rc_image imgs[] = {
        solid(&fix->a, 8, 8,   RC_PIXEL_FORMAT_RGBA8, 0xFF010101u),  // small, sorts last
        solid(&fix->a, 40, 40, RC_PIXEL_FORMAT_RGBA8, 0xFF020202u),  // large, sorts first
        solid(&fix->a, 8, 8,   RC_PIXEL_FORMAT_RGBA8, 0xFF030303u),
    };
    rc_view_image v = RC_VIEW(imgs);
    rc_vec2i container = rc_vec2i_make(64, 64);

    rc_image_pack_result r = rc_image_pack(v, container, 1, &fix->a, fix->scratch);
    RC_CHECK_TRUE(r.image.data.data != NULL);
    check_atlas(v, r.positions, container, 1);
    RC_CHECK(sample_center(r.image, rc_span_vec2i_get(r.positions, 0), imgs[0].size), ==, 0xFF010101u);
    RC_CHECK(sample_center(r.image, rc_span_vec2i_get(r.positions, 1), imgs[1].size), ==, 0xFF020202u);
    RC_CHECK(sample_center(r.image, rc_span_vec2i_get(r.positions, 2), imgs[2].size), ==, 0xFF030303u);
}

RC_TEST_STEP(image_pack, gaps_cleared, fix)
{
    // a small image in a large atlas leaves the rest cleared (zero)
    rc_image imgs[] = {solid(&fix->a, 8, 8, RC_PIXEL_FORMAT_RGBA8, 0xFFFFFFFFu)};
    rc_view_image v = RC_VIEW(imgs);
    rc_image_pack_result r = rc_image_pack(v, rc_vec2i_make(64, 64), 0, &fix->a, fix->scratch);
    RC_CHECK_TRUE(r.image.data.data != NULL);
    // placed at (0,0) spanning 8x8; a pixel far from it reads as zero
    RC_CHECK(rc_image_get_pixel(r.image, 60, 60), ==, 0x00000000u);
}

/* ---- auto-sizing (size == {0,0}) ---- */

RC_TEST_STEP(image_pack, auto_size_basic, fix)
{
    // three 32x32 (area 3072) fit the first square (64x64); no growth needed
    rc_image imgs[] = {
        solid(&fix->a, 32, 32, RC_PIXEL_FORMAT_RGBA8, 0xFF0000FFu),
        solid(&fix->a, 32, 32, RC_PIXEL_FORMAT_RGBA8, 0xFF00FF00u),
        solid(&fix->a, 32, 32, RC_PIXEL_FORMAT_RGBA8, 0xFFFF0000u),
    };
    rc_view_image v = RC_VIEW(imgs);
    rc_image_pack_result r = rc_image_pack(v, rc_vec2i_make(0, 0), 0, &fix->a, fix->scratch);
    RC_CHECK_TRUE(r.image.data.data != NULL);
    RC_CHECK(r.image.size.x, ==, 64);
    RC_CHECK(r.image.size.y, ==, 64);
    check_atlas(v, r.positions, r.image.size, 0);
    for (uint32_t i = 0; i < 3; i++)
        RC_CHECK(sample_center(r.image, rc_span_vec2i_get(r.positions, i), imgs[i].size),
                 ==, rc_image_get_pixel(imgs[i], imgs[i].size.x / 2, imgs[i].size.y / 2));
}

RC_TEST_STEP(image_pack, auto_size_grows, fix)
{
    // a 400x50 image (area 20000 -> start square 256) is wider than the square,
    // so width doubles once to 512: container becomes 512x256
    rc_image imgs[] = {solid(&fix->a, 400, 50, RC_PIXEL_FORMAT_R8, 0x77u)};
    rc_view_image v = RC_VIEW(imgs);
    rc_image_pack_result r = rc_image_pack(v, rc_vec2i_make(0, 0), 0, &fix->a, fix->scratch);
    RC_CHECK_TRUE(r.image.data.data != NULL);
    RC_CHECK(r.image.size.x, ==, 512);
    RC_CHECK(r.image.size.y, ==, 256);
    rc_vec2i p0 = rc_span_vec2i_get(r.positions, 0);
    RC_CHECK(sample_center(r.image, p0, imgs[0].size), ==, 0xFF777777u);
}

RC_TEST_STEP(image_pack, auto_size_no_arena_leak, fix)
{
    // auto-sizing retries at growing containers (this image needs two failed
    // attempts before 512x256 fits).  The failed attempts must not leak arena
    // space, so the auto pack must consume exactly as much arena as a single
    // direct pack at the resulting size.
    rc_image imgs[] = {solid(&fix->a, 400, 50, RC_PIXEL_FORMAT_R8, 0x77u)};
    rc_view_image v = RC_VIEW(imgs);

    uint32_t b1 = fix->a.top;
    rc_image_pack_result r = rc_image_pack(v, rc_vec2i_make(0, 0), 0, &fix->a, fix->scratch);
    RC_CHECK_TRUE(r.image.data.data != NULL);
    uint32_t auto_used = fix->a.top - b1;

    rc_arena a2 = rc_arena_make_default();
    uint32_t b2 = a2.top;
    rc_image_pack_result r2 = rc_image_pack(v, r.image.size, 0, &a2, fix->scratch);
    RC_CHECK_TRUE(r2.image.data.data != NULL);
    uint32_t direct_used = a2.top - b2;
    rc_arena_deinit(&a2);

    RC_CHECK(auto_used, ==, direct_used);
}

RC_TEST_STEP(image_pack, auto_size_many_grows, fix)
{
    // a wide item forces growth beyond the first square even with several images
    rc_image imgs[] = {
        solid(&fix->a, 200, 30, RC_PIXEL_FORMAT_RGBA8, 0xFF0000FFu),
        solid(&fix->a, 30, 30,  RC_PIXEL_FORMAT_RGBA8, 0xFF00FF00u),
        solid(&fix->a, 30, 30,  RC_PIXEL_FORMAT_RGBA8, 0xFFFF0000u),
        solid(&fix->a, 30, 30,  RC_PIXEL_FORMAT_RGBA8, 0xFFFFFF00u),
    };
    rc_view_image v = RC_VIEW(imgs);
    rc_image_pack_result r = rc_image_pack(v, rc_vec2i_make(0, 0), 2, &fix->a, fix->scratch);
    RC_CHECK_TRUE(r.image.data.data != NULL);
    RC_CHECK(r.image.size.x >= 200, ==, true);
    check_atlas(v, r.positions, r.image.size, 2);
    for (uint32_t i = 0; i < 4; i++)
        RC_CHECK(sample_center(r.image, rc_span_vec2i_get(r.positions, i), imgs[i].size),
                 ==, rc_image_get_pixel(imgs[i], imgs[i].size.x / 2, imgs[i].size.y / 2));
}
