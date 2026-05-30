#include "richc/math/vec3i.h"
#include "richc/test.h"

RC_TEST(vec3i, construct)
{
    rc_vec3i a = rc_vec3i_make(1, 2, 3);
    RC_CHECK(a.x, ==, 1);
    RC_CHECK(a.y, ==, 2);
    RC_CHECK(a.z, ==, 3);
    RC_CHECK_TRUE(rc_vec3i_is_equal(rc_vec3i_make_zero(), rc_vec3i_make(0, 0, 0)));
    RC_CHECK_TRUE(rc_vec3i_is_equal(rc_vec3i_make_unitz(), rc_vec3i_make(0, 0, 1)));

    int32_t raw[] = {7, 8, 9};
    RC_CHECK_TRUE(rc_vec3i_is_equal(rc_vec3i_from_i32s(raw), rc_vec3i_make(7, 8, 9)));
    RC_CHECK_TRUE(rc_vec3i_is_equal(rc_vec3i_from_vec2i(rc_vec2i_make(4, 5), 6),
                                    rc_vec3i_make(4, 5, 6)));
    RC_CHECK_TRUE(rc_vec3i_as_i32s(&a) == &a.x);
}

RC_TEST(vec3i, arithmetic)
{
    rc_vec3i a = rc_vec3i_make(1, 2, 3);
    rc_vec3i b = rc_vec3i_make(10, 20, 30);
    RC_CHECK_TRUE(rc_vec3i_is_equal(rc_vec3i_add(a, b), rc_vec3i_make(11, 22, 33)));
    RC_CHECK_TRUE(rc_vec3i_is_equal(rc_vec3i_add3(a, b, a), rc_vec3i_make(12, 24, 36)));
    RC_CHECK_TRUE(rc_vec3i_is_equal(rc_vec3i_sub(b, a), rc_vec3i_make(9, 18, 27)));
    RC_CHECK_TRUE(rc_vec3i_is_equal(rc_vec3i_scalar_mul(a, 2), rc_vec3i_make(2, 4, 6)));
    RC_CHECK_TRUE(rc_vec3i_is_equal(rc_vec3i_scalar_div(b, 10), rc_vec3i_make(1, 2, 3)));
    RC_CHECK_TRUE(rc_vec3i_is_equal(rc_vec3i_component_mul(a, b), rc_vec3i_make(10, 40, 90)));
    RC_CHECK_TRUE(rc_vec3i_is_equal(rc_vec3i_negate(a), rc_vec3i_make(-1, -2, -3)));
}

RC_TEST(vec3i, component_min_max)
{
    rc_vec3i a = rc_vec3i_make(1, 2, 3);
    rc_vec3i b = rc_vec3i_make(0, 5, 1);
    RC_CHECK_TRUE(rc_vec3i_is_equal(rc_vec3i_component_min(a, b), rc_vec3i_make(0, 2, 1)));
    RC_CHECK_TRUE(rc_vec3i_is_equal(rc_vec3i_component_max(a, b), rc_vec3i_make(1, 5, 3)));
}

RC_TEST(vec3i, products)
{
    rc_vec3i a = rc_vec3i_make(1, 2, 3);
    rc_vec3i b = rc_vec3i_make(4, 5, 6);
    RC_CHECK(rc_vec3i_dot(a, b), ==, 32);                         // 4 + 10 + 18
    RC_CHECK(rc_vec3i_lengthsqr(rc_vec3i_make(1, 2, 2)), ==, 9);  // 1 + 4 + 4
    // x cross y == z
    RC_CHECK_TRUE(rc_vec3i_is_equal(
        rc_vec3i_cross(rc_vec3i_make_unitx(), rc_vec3i_make_unity()),
        rc_vec3i_make_unitz()));
    RC_CHECK_TRUE(rc_vec3i_is_equal(rc_vec3i_cross(a, b), rc_vec3i_make(-3, 6, -3)));
}
