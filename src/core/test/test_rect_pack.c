#include "richc/arena.h"
#include "richc/rect_pack.h"
#include "richc/test.h"

RC_TEST_GROUP_DATA(rect_pack) {
    rc_arena a;
    rc_arena scratch;
};

RC_TEST_GROUP_INIT(rect_pack, fix)
{
    fix->a       = rc_arena_make_default();
    fix->scratch = rc_arena_make_default();
}

RC_TEST_GROUP_DEINIT(rect_pack, fix)
{
    rc_arena_deinit(&fix->a);
    rc_arena_deinit(&fix->scratch);
}

/* ---- helpers ---- */

// True if a placed rectangle (position + size), inflated by spacing on all
// sides, overlaps another placed rectangle.  The half-open convention of
// rc_box2i_intersects means touching edges do not count as overlap.
static bool overlaps_with_spacing(rc_vec2i pa, rc_vec2i sa,
                                  rc_vec2i pb, rc_vec2i sb, int32_t spacing)
{
    rc_box2i a = rc_box2i_make_with_margin(pa, rc_vec2i_make(pa.x + sa.x, pa.y + sa.y), spacing);
    rc_box2i b = rc_box2i_make_pos_size(pb, sb);
    return rc_box2i_intersects(a, b);
}

// Assert no pair of the placed rectangles violates the spacing gap, and that
// each lies fully within the container.
static void check_valid_packing(rc_view_vec2i sizes, rc_span_vec2i positions,
                                rc_vec2i container, int32_t spacing)
{
    for (uint32_t i = 0; i < sizes.num; i++) {
        rc_vec2i p = rc_span_vec2i_get(positions, i);
        rc_vec2i s = rc_view_vec2i_get(sizes, i);
        RC_CHECK(p.x >= 0, ==, true);
        RC_CHECK(p.y >= 0, ==, true);
        RC_CHECK(p.x + s.x <= container.x, ==, true);
        RC_CHECK(p.y + s.y <= container.y, ==, true);
        for (uint32_t j = i + 1; j < sizes.num; j++)
            RC_CHECK(overlaps_with_spacing(p, s, rc_span_vec2i_get(positions, j),
                                           rc_view_vec2i_get(sizes, j), spacing),
                     ==, false);
    }
}

/* ---- rc_rect_pack_all: edge cases ---- */

RC_TEST(rect_pack, all_empty)
{
    rc_arena a = rc_arena_make_default();
    rc_arena scratch = rc_arena_make_default();

    rc_span_vec2i r = rc_rect_pack_all(rc_vec2i_make(64, 64), 0,
                                        (rc_view_vec2i) {0}, &a, scratch);
    RC_CHECK_FALSE(rc_span_vec2i_is_valid(r));
    RC_CHECK(r.num, ==, 0u);

    rc_arena_deinit(&a);
    rc_arena_deinit(&scratch);
}

RC_TEST_STEP(rect_pack, all_single, fix)
{
    rc_vec2i sizes[] = {rc_vec2i_make(10, 20)};
    rc_view_vec2i sv = RC_VIEW(sizes);
    rc_span_vec2i r = rc_rect_pack_all(rc_vec2i_make(64, 64), 0, sv, &fix->a, fix->scratch);
    RC_CHECK(r.num, ==, 1u);
    // first placement goes to the container origin
    rc_vec2i p0 = rc_span_vec2i_get(r, 0);
    RC_CHECK(p0.x, ==, 0);
    RC_CHECK(p0.y, ==, 0);
}

RC_TEST_STEP(rect_pack, all_exact_tiling, fix)
{
    // four 32x32 tiles exactly fill a 64x64 container with no spacing
    rc_vec2i sizes[] = {
        rc_vec2i_make(32, 32), rc_vec2i_make(32, 32),
        rc_vec2i_make(32, 32), rc_vec2i_make(32, 32)
    };
    rc_view_vec2i sv = RC_VIEW(sizes);
    rc_vec2i container = rc_vec2i_make(64, 64);
    rc_span_vec2i r = rc_rect_pack_all(container, 0, sv, &fix->a, fix->scratch);
    RC_CHECK(r.num, ==, 4u);
    check_valid_packing(sv, r, container, 0);
}

RC_TEST_STEP(rect_pack, all_overflow_too_big, fix)
{
    // a single rectangle larger than the container fails the whole pack
    rc_vec2i sizes[] = {rc_vec2i_make(100, 10)};
    rc_view_vec2i sv = RC_VIEW(sizes);
    rc_span_vec2i r = rc_rect_pack_all(rc_vec2i_make(64, 64), 0, sv, &fix->a, fix->scratch);
    RC_CHECK_FALSE(rc_span_vec2i_is_valid(r));
    RC_CHECK(r.num, ==, 0u);
}

RC_TEST_STEP(rect_pack, all_overflow_too_many, fix)
{
    // total area exceeds the container: five 32x32 tiles do not fit in 64x64
    rc_vec2i sizes[] = {
        rc_vec2i_make(32, 32), rc_vec2i_make(32, 32), rc_vec2i_make(32, 32),
        rc_vec2i_make(32, 32), rc_vec2i_make(32, 32)
    };
    rc_view_vec2i sv = RC_VIEW(sizes);
    rc_span_vec2i r = rc_rect_pack_all(rc_vec2i_make(64, 64), 0, sv, &fix->a, fix->scratch);
    RC_CHECK_FALSE(rc_span_vec2i_is_valid(r));
    RC_CHECK(r.num, ==, 0u);
}

RC_TEST_STEP(rect_pack, all_failure_leaves_arena_clean, fix)
{
    // a failed pack must not leave its result array stranded in arena: the bump
    // pointer must return to where it was before the call
    uint32_t before = fix->a.top;
    rc_vec2i big[] = {rc_vec2i_make(100, 100)};
    rc_view_vec2i sv = RC_VIEW(big);
    rc_span_vec2i r = rc_rect_pack_all(rc_vec2i_make(64, 64), 0, sv, &fix->a, fix->scratch);
    RC_CHECK_FALSE(rc_span_vec2i_is_valid(r));
    RC_CHECK(fix->a.top, ==, before);
}

/* ---- rc_rect_pack_all: density and ordering ---- */

RC_TEST_STEP(rect_pack, all_indexed_by_original_order, fix)
{
    // mixed sizes; the result must be indexed by the original sizes order even
    // though packing sorts internally by decreasing max-side
    rc_vec2i sizes[] = {
        rc_vec2i_make(8, 8),    // small, would sort last
        rc_vec2i_make(40, 40),  // large, would sort first
        rc_vec2i_make(8, 8)
    };
    rc_view_vec2i sv = RC_VIEW(sizes);
    rc_vec2i container = rc_vec2i_make(64, 64);
    rc_span_vec2i r = rc_rect_pack_all(container, 1, sv, &fix->a, fix->scratch);
    RC_CHECK(r.num, ==, 3u);
    check_valid_packing(sv, r, container, 1);
}

RC_TEST_STEP(rect_pack, all_many_no_overlap, fix)
{
    // a spread of sizes that pack densely; verify all fit with the gap honoured
    rc_vec2i sizes[] = {
        rc_vec2i_make(30, 20), rc_vec2i_make(20, 30), rc_vec2i_make(15, 15),
        rc_vec2i_make(40, 10), rc_vec2i_make(10, 40), rc_vec2i_make(25, 25),
        rc_vec2i_make(12, 8),  rc_vec2i_make(8, 12)
    };
    rc_view_vec2i sv = RC_VIEW(sizes);
    rc_vec2i container = rc_vec2i_make(128, 128);
    rc_span_vec2i r = rc_rect_pack_all(container, 2, sv, &fix->a, fix->scratch);
    RC_CHECK(r.num, ==, 8u);
    check_valid_packing(sv, r, container, 2);
}

/* ---- rc_rect_pack_make / _add: incremental ---- */

RC_TEST_STEP(rect_pack, incremental_stable_placements, fix)
{
    rc_rect_pack p = rc_rect_pack_make(rc_vec2i_make(64, 64), 1, &fix->a);

    rc_rect_pack_result a = rc_rect_pack_add(&p, rc_vec2i_make(20, 20), &fix->a);
    RC_CHECK_TRUE(a.placed);
    RC_CHECK(a.pos.x, ==, 0);
    RC_CHECK(a.pos.y, ==, 0);

    rc_rect_pack_result b = rc_rect_pack_add(&p, rc_vec2i_make(20, 20), &fix->a);
    RC_CHECK_TRUE(b.placed);

    rc_rect_pack_result c = rc_rect_pack_add(&p, rc_vec2i_make(10, 10), &fix->a);
    RC_CHECK_TRUE(c.placed);

    // adding more must not have moved the first placement
    RC_CHECK(a.pos.x, ==, 0);
    RC_CHECK(a.pos.y, ==, 0);

    // none of the three may overlap (1px gap)
    RC_CHECK(overlaps_with_spacing(a.pos, rc_vec2i_make(20, 20), b.pos, rc_vec2i_make(20, 20), 1),
             ==, false);
    RC_CHECK(overlaps_with_spacing(a.pos, rc_vec2i_make(20, 20), c.pos, rc_vec2i_make(10, 10), 1),
             ==, false);
    RC_CHECK(overlaps_with_spacing(b.pos, rc_vec2i_make(20, 20), c.pos, rc_vec2i_make(10, 10), 1),
             ==, false);
}

RC_TEST_STEP(rect_pack, incremental_fill_until_full, fix)
{
    // 32x32 cells in 64x64 with no spacing: exactly four fit, the fifth fails
    rc_rect_pack p = rc_rect_pack_make(rc_vec2i_make(64, 64), 0, &fix->a);
    for (int i = 0; i < 4; i++) {
        rc_rect_pack_result r = rc_rect_pack_add(&p, rc_vec2i_make(32, 32), &fix->a);
        RC_CHECK_TRUE(r.placed);
    }
    rc_rect_pack_result overflow = rc_rect_pack_add(&p, rc_vec2i_make(32, 32), &fix->a);
    RC_CHECK_FALSE(overflow.placed);
    RC_CHECK(overflow.pos.x, ==, 0);
    RC_CHECK(overflow.pos.y, ==, 0);

    // once full, even a 1x1 rectangle no longer fits
    rc_rect_pack_result tiny = rc_rect_pack_add(&p, rc_vec2i_make(1, 1), &fix->a);
    RC_CHECK_FALSE(tiny.placed);
}

RC_TEST_STEP(rect_pack, incremental_exact_container, fix)
{
    // a single rectangle exactly the container size fits
    rc_rect_pack p = rc_rect_pack_make(rc_vec2i_make(50, 40), 0, &fix->a);
    rc_rect_pack_result r = rc_rect_pack_add(&p, rc_vec2i_make(50, 40), &fix->a);
    RC_CHECK_TRUE(r.placed);
    RC_CHECK(r.pos.x, ==, 0);
    RC_CHECK(r.pos.y, ==, 0);
    // the container is now full
    rc_rect_pack_result r2 = rc_rect_pack_add(&p, rc_vec2i_make(1, 1), &fix->a);
    RC_CHECK_FALSE(r2.placed);
}
