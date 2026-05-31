#include "richc/math/box2f.h"
#include "richc/test.h"

static bool box_eq(rc_box2f a, rc_box2f b)
{
    return rc_vec2f_is_equal(rc_box2f_min(a), rc_box2f_min(b))
        && rc_vec2f_is_equal(rc_box2f_max(a), rc_box2f_max(b));
}

RC_TEST(box2f, make)
{
    // make sorts the two corners component-wise
    rc_box2f b = rc_box2f_make(rc_vec2f_make(5.0f, 1.0f), rc_vec2f_make(2.0f, 8.0f));
    RC_CHECK_TRUE(box_eq(b, rc_box2f_make(rc_vec2f_make(2.0f, 1.0f), rc_vec2f_make(5.0f, 8.0f))));
}

RC_TEST(box2f, make_pos_size)
{
    rc_box2f b = rc_box2f_make_pos_size(rc_vec2f_make(3.0f, 4.0f), rc_vec2f_make(10.0f, 20.0f));
    RC_CHECK(rc_box2f_min(b), ==, rc_vec2f_make(3.0f, 4.0f));
    RC_CHECK(rc_box2f_max(b), ==, rc_vec2f_make(13.0f, 24.0f));
    RC_CHECK(rc_box2f_size(b), ==, rc_vec2f_make(10.0f, 20.0f));
}

RC_TEST(box2f, make_with_margin)
{
    rc_box2f b = rc_box2f_make_with_margin(rc_vec2f_make(2.0f, 3.0f), rc_vec2f_make(5.0f, 7.0f), 1.0f);
    RC_CHECK_TRUE(box_eq(b, rc_box2f_make(rc_vec2f_make(1.0f, 2.0f), rc_vec2f_make(6.0f, 8.0f))));
}

RC_TEST(box2f, contains)
{
    rc_box2f outer = rc_box2f_make_pos_size(rc_vec2f_make(0.0f, 0.0f), rc_vec2f_make(10.0f, 10.0f));
    rc_box2f inner = rc_box2f_make_pos_size(rc_vec2f_make(2.0f, 2.0f), rc_vec2f_make(3.0f, 3.0f));
    RC_CHECK_TRUE(rc_box2f_contains(outer, inner));
    RC_CHECK_TRUE(rc_box2f_contains(outer, outer));    // a box contains itself
    RC_CHECK_FALSE(rc_box2f_contains(inner, outer));
    // a box poking past the max edge is not contained
    rc_box2f poke = rc_box2f_make_pos_size(rc_vec2f_make(8.0f, 8.0f), rc_vec2f_make(5.0f, 5.0f));  // max (13,13)
    RC_CHECK_FALSE(rc_box2f_contains(outer, poke));
}

RC_TEST(box2f, intersects)
{
    rc_box2f a = rc_box2f_make_pos_size(rc_vec2f_make(0.0f, 0.0f), rc_vec2f_make(10.0f, 10.0f));   // [0,10)
    rc_box2f overlap = rc_box2f_make_pos_size(rc_vec2f_make(5.0f, 5.0f), rc_vec2f_make(10.0f, 10.0f));
    RC_CHECK_TRUE(rc_box2f_intersects(a, overlap));
    // touching edge (a.max.x == touch.min.x == 10) does not count
    rc_box2f touch = rc_box2f_make_pos_size(rc_vec2f_make(10.0f, 0.0f), rc_vec2f_make(5.0f, 10.0f));
    RC_CHECK_FALSE(rc_box2f_intersects(a, touch));
    rc_box2f far = rc_box2f_make_pos_size(rc_vec2f_make(20.0f, 20.0f), rc_vec2f_make(5.0f, 5.0f));
    RC_CHECK_FALSE(rc_box2f_intersects(a, far));
}

RC_TEST(box2f, contains_point)
{
    rc_box2f b = rc_box2f_make_pos_size(rc_vec2f_make(0.0f, 0.0f), rc_vec2f_make(10.0f, 10.0f));   // [0,10)
    RC_CHECK_TRUE(rc_box2f_contains_point(b, rc_vec2f_make(0.0f, 0.0f)));     // min inclusive
    RC_CHECK_TRUE(rc_box2f_contains_point(b, rc_vec2f_make(9.0f, 9.0f)));
    RC_CHECK_FALSE(rc_box2f_contains_point(b, rc_vec2f_make(10.0f, 5.0f)));   // max exclusive
    RC_CHECK_FALSE(rc_box2f_contains_point(b, rc_vec2f_make(5.0f, 10.0f)));
    RC_CHECK_FALSE(rc_box2f_contains_point(b, rc_vec2f_make(-1.0f, 5.0f)));
}

RC_TEST(box2f, union_expand)
{
    rc_box2f a = rc_box2f_make(rc_vec2f_make(0.0f, 0.0f), rc_vec2f_make(5.0f, 5.0f));
    rc_box2f b = rc_box2f_make(rc_vec2f_make(3.0f, 3.0f), rc_vec2f_make(10.0f, 8.0f));
    RC_CHECK_TRUE(box_eq(rc_box2f_union(a, b),
                         rc_box2f_make(rc_vec2f_make(0.0f, 0.0f), rc_vec2f_make(10.0f, 8.0f))));

    rc_box2f e = rc_box2f_expand(a, rc_vec2f_make(-2.0f, 7.0f));
    RC_CHECK_TRUE(box_eq(e, rc_box2f_make(rc_vec2f_make(-2.0f, 0.0f), rc_vec2f_make(5.0f, 7.0f))));
}
