#include "richc/math/box2i.h"
#include "richc/test.h"

static bool box_eq(rc_box2i a, rc_box2i b)
{
    return rc_vec2i_is_equal(rc_box2i_min(a), rc_box2i_min(b))
        && rc_vec2i_is_equal(rc_box2i_max(a), rc_box2i_max(b));
}

RC_TEST(box2i, make)
{
    // make sorts the two corners component-wise
    rc_box2i b = rc_box2i_make(rc_vec2i_make(5, 1), rc_vec2i_make(2, 8));
    RC_CHECK_TRUE(box_eq(b, rc_box2i_make(rc_vec2i_make(2, 1), rc_vec2i_make(5, 8))));
}

RC_TEST(box2i, make_pos_size)
{
    rc_box2i b = rc_box2i_make_pos_size(rc_vec2i_make(3, 4), rc_vec2i_make(10, 20));
    RC_CHECK_TRUE(rc_vec2i_is_equal(rc_box2i_min(b), rc_vec2i_make(3, 4)));
    RC_CHECK_TRUE(rc_vec2i_is_equal(rc_box2i_max(b), rc_vec2i_make(13, 24)));
    RC_CHECK_TRUE(rc_vec2i_is_equal(rc_box2i_size(b), rc_vec2i_make(10, 20)));
}

RC_TEST(box2i, make_with_margin)
{
    rc_box2i b = rc_box2i_make_with_margin(rc_vec2i_make(2, 3), rc_vec2i_make(5, 7), 1);
    RC_CHECK_TRUE(box_eq(b, rc_box2i_make(rc_vec2i_make(1, 2), rc_vec2i_make(6, 8))));
}

RC_TEST(box2i, contains)
{
    rc_box2i outer = rc_box2i_make_pos_size(rc_vec2i_make(0, 0), rc_vec2i_make(10, 10));
    rc_box2i inner = rc_box2i_make_pos_size(rc_vec2i_make(2, 2), rc_vec2i_make(3, 3));
    RC_CHECK_TRUE(rc_box2i_contains(outer, inner));
    RC_CHECK_TRUE(rc_box2i_contains(outer, outer));    // a box contains itself
    RC_CHECK_FALSE(rc_box2i_contains(inner, outer));
    // a box poking past the max edge is not contained
    rc_box2i poke = rc_box2i_make_pos_size(rc_vec2i_make(8, 8), rc_vec2i_make(5, 5));  // max (13,13)
    RC_CHECK_FALSE(rc_box2i_contains(outer, poke));
}

RC_TEST(box2i, intersects)
{
    rc_box2i a = rc_box2i_make_pos_size(rc_vec2i_make(0, 0), rc_vec2i_make(10, 10));   // [0,10)
    rc_box2i overlap = rc_box2i_make_pos_size(rc_vec2i_make(5, 5), rc_vec2i_make(10, 10));
    RC_CHECK_TRUE(rc_box2i_intersects(a, overlap));
    // touching edge (a.max.x == touch.min.x == 10) does not count
    rc_box2i touch = rc_box2i_make_pos_size(rc_vec2i_make(10, 0), rc_vec2i_make(5, 10));
    RC_CHECK_FALSE(rc_box2i_intersects(a, touch));
    rc_box2i far = rc_box2i_make_pos_size(rc_vec2i_make(20, 20), rc_vec2i_make(5, 5));
    RC_CHECK_FALSE(rc_box2i_intersects(a, far));
}

RC_TEST(box2i, contains_point)
{
    rc_box2i b = rc_box2i_make_pos_size(rc_vec2i_make(0, 0), rc_vec2i_make(10, 10));   // [0,10)
    RC_CHECK_TRUE(rc_box2i_contains_point(b, rc_vec2i_make(0, 0)));     // min inclusive
    RC_CHECK_TRUE(rc_box2i_contains_point(b, rc_vec2i_make(9, 9)));
    RC_CHECK_FALSE(rc_box2i_contains_point(b, rc_vec2i_make(10, 5)));   // max exclusive
    RC_CHECK_FALSE(rc_box2i_contains_point(b, rc_vec2i_make(5, 10)));
    RC_CHECK_FALSE(rc_box2i_contains_point(b, rc_vec2i_make(-1, 5)));
}

RC_TEST(box2i, union_expand)
{
    rc_box2i a = rc_box2i_make(rc_vec2i_make(0, 0), rc_vec2i_make(5, 5));
    rc_box2i b = rc_box2i_make(rc_vec2i_make(3, 3), rc_vec2i_make(10, 8));
    RC_CHECK_TRUE(box_eq(rc_box2i_union(a, b),
                         rc_box2i_make(rc_vec2i_make(0, 0), rc_vec2i_make(10, 8))));

    rc_box2i e = rc_box2i_expand(a, rc_vec2i_make(-2, 7));
    RC_CHECK_TRUE(box_eq(e, rc_box2i_make(rc_vec2i_make(-2, 0), rc_vec2i_make(5, 7))));
}
