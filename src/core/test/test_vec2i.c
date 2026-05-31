#include "richc/math/vec2i.h"
#include "richc/test.h"

RC_TEST(vec2i, construct)
{
    rc_vec2i a = rc_vec2i_make(3, 4);
    RC_CHECK(a, ==, rc_vec2i_make(3, 4));
    RC_CHECK(rc_vec2i_make_zero(), ==, rc_vec2i_make(0, 0));
    RC_CHECK(rc_vec2i_make_unitx(), ==, rc_vec2i_make(1, 0));
    RC_CHECK(rc_vec2i_make_unity(), ==, rc_vec2i_make(0, 1));

    int32_t raw[] = {5, 6};
    RC_CHECK(rc_vec2i_from_i32s(raw), ==, rc_vec2i_make(5, 6));
    RC_CHECK_TRUE(rc_vec2i_as_i32s(&a) == &a.x);
}

RC_TEST(vec2i, arithmetic)
{
    rc_vec2i a = rc_vec2i_make(1, 2);
    rc_vec2i b = rc_vec2i_make(10, 20);
    RC_CHECK(rc_vec2i_add(a, b), ==, rc_vec2i_make(11, 22));
    RC_CHECK(rc_vec2i_add3(a, b, a), ==, rc_vec2i_make(12, 24));
    RC_CHECK(rc_vec2i_add4(a, b, a, b), ==, rc_vec2i_make(22, 44));
    RC_CHECK(rc_vec2i_sub(b, a), ==, rc_vec2i_make(9, 18));
    RC_CHECK(rc_vec2i_scalar_mul(a, 3), ==, rc_vec2i_make(3, 6));
    RC_CHECK(rc_vec2i_scalar_div(b, 5), ==, rc_vec2i_make(2, 4));
    RC_CHECK(rc_vec2i_component_mul(a, b), ==, rc_vec2i_make(10, 40));
    RC_CHECK(rc_vec2i_negate(a), ==, rc_vec2i_make(-1, -2));
}

RC_TEST(vec2i, component_min_max)
{
    rc_vec2i a = rc_vec2i_make(1, 8);
    rc_vec2i b = rc_vec2i_make(5, 3);
    RC_CHECK(rc_vec2i_component_min(a, b), ==, rc_vec2i_make(1, 3));
    RC_CHECK(rc_vec2i_component_max(a, b), ==, rc_vec2i_make(5, 8));
}

RC_TEST(vec2i, products)
{
    rc_vec2i a = rc_vec2i_make(1, 2);
    rc_vec2i b = rc_vec2i_make(3, 4);
    RC_CHECK(rc_vec2i_dot(a, b), ==, 11);                       // 1*3 + 2*4
    RC_CHECK(rc_vec2i_wedge(a, b), ==, -2);                     // 1*4 - 3*2
    RC_CHECK(rc_vec2i_lengthsqr(rc_vec2i_make(3, 4)), ==, 25);  // 9 + 16
}

RC_TEST(vec2i, perp)
{
    rc_vec2i a = rc_vec2i_make(2, 3);
    RC_CHECK(rc_vec2i_perp(a), ==, rc_vec2i_make(-3, 2));
    RC_CHECK(rc_vec2i_dot(a, rc_vec2i_perp(a)), ==, 0);        // perp is orthogonal
}
