#include "richc/math/vec2f.h"
#include "richc/test.h"

// sin(30 deg) = 1/2, cos(30 deg) = sqrt(3)/2 = 0.86602540
#define RC_PI_OVER_6 0.52359878f

RC_TEST(vec2f, construct)
{
    rc_vec2f a = rc_vec2f_make(3.0f, 4.0f);
    RC_CHECK(a, ==, rc_vec2f_make(3.0f, 4.0f));
    RC_CHECK(rc_vec2f_make_zero(), ==, rc_vec2f_make(0.0f, 0.0f));
    RC_CHECK(rc_vec2f_make_unitx(), ==, rc_vec2f_make(1.0f, 0.0f));
    RC_CHECK(rc_vec2f_make_unity(), ==, rc_vec2f_make(0.0f, 1.0f));

    RC_CHECK(rc_vec2f_make_sincos(RC_PI_OVER_6), ~=, rc_vec2f_make(0.5f, 0.8660254f));
    RC_CHECK(rc_vec2f_make_cossin(RC_PI_OVER_6), ~=, rc_vec2f_make(0.8660254f, 0.5f));

    float raw[] = {5.0f, 6.0f};
    RC_CHECK(rc_vec2f_from_floats(raw), ==, rc_vec2f_make(5.0f, 6.0f));
    RC_CHECK(rc_vec2f_from_vec2i(rc_vec2i_make(7, 8)), ==, rc_vec2f_make(7.0f, 8.0f));
    RC_CHECK_TRUE(rc_vec2f_as_floats(&a) == &a.x);
}

RC_TEST(vec2f, arithmetic)
{
    rc_vec2f a = rc_vec2f_make(1.0f, 2.0f);
    rc_vec2f b = rc_vec2f_make(10.0f, 20.0f);
    RC_CHECK(rc_vec2f_add(a, b), ==, rc_vec2f_make(11.0f, 22.0f));
    RC_CHECK(rc_vec2f_add3(a, b, a), ==, rc_vec2f_make(12.0f, 24.0f));
    RC_CHECK(rc_vec2f_add4(a, b, a, b), ==, rc_vec2f_make(22.0f, 44.0f));
    RC_CHECK(rc_vec2f_sub(b, a), ==, rc_vec2f_make(9.0f, 18.0f));
    RC_CHECK(rc_vec2f_scalar_mul(a, 3.0f), ==, rc_vec2f_make(3.0f, 6.0f));
    RC_CHECK(rc_vec2f_scalar_div(b, 5.0f), ==, rc_vec2f_make(2.0f, 4.0f));
    RC_CHECK(rc_vec2f_component_mul(a, b), ==, rc_vec2f_make(10.0f, 40.0f));
    RC_CHECK(rc_vec2f_negate(a), ==, rc_vec2f_make(-1.0f, -2.0f));
}

RC_TEST(vec2f, component_ops)
{
    rc_vec2f a = rc_vec2f_make(1.0f, 8.0f);
    rc_vec2f b = rc_vec2f_make(5.0f, 3.0f);
    RC_CHECK(rc_vec2f_component_min(a, b), ==, rc_vec2f_make(1.0f, 3.0f));
    RC_CHECK(rc_vec2f_component_max(a, b), ==, rc_vec2f_make(5.0f, 8.0f));

    rc_vec2f c = rc_vec2f_make(1.7f, -1.2f);
    RC_CHECK(rc_vec2f_component_floor(c), ==, rc_vec2f_make(1.0f, -2.0f));
    RC_CHECK(rc_vec2f_component_ceil(c), ==, rc_vec2f_make(2.0f, -1.0f));
    RC_CHECK(rc_vec2f_component_abs(rc_vec2f_make(-1.5f, 2.5f)), ==, rc_vec2f_make(1.5f, 2.5f));
}

RC_TEST(vec2f, lerp)
{
    rc_vec2f a = rc_vec2f_make(0.0f, 0.0f);
    rc_vec2f b = rc_vec2f_make(10.0f, 20.0f);
    RC_CHECK(rc_vec2f_lerp(a, b, 0.0f), ==, a);
    RC_CHECK(rc_vec2f_lerp(a, b, 1.0f), ==, b);
    RC_CHECK(rc_vec2f_lerp(a, b, 0.25f), ~=, rc_vec2f_make(2.5f, 5.0f));
}

RC_TEST(vec2f, products)
{
    rc_vec2f a = rc_vec2f_make(1.0f, 2.0f);
    rc_vec2f b = rc_vec2f_make(3.0f, 4.0f);
    RC_CHECK(rc_vec2f_dot(a, b), ~=, 11.0f);     // 1*3 + 2*4
    RC_CHECK(rc_vec2f_wedge(a, b), ~=, -2.0f);   // 1*4 - 3*2

    rc_vec2f p = rc_vec2f_perp(rc_vec2f_make(2.0f, 3.0f));
    RC_CHECK(p, ==, rc_vec2f_make(-3.0f, 2.0f));
    RC_CHECK(rc_vec2f_dot(rc_vec2f_make(2.0f, 3.0f), p), ~=, 0.0f);  // perp is orthogonal
}

RC_TEST(vec2f, length_normalize)
{
    rc_vec2f a = rc_vec2f_make(3.0f, 4.0f);     // classic 3-4-5 triangle
    RC_CHECK(rc_vec2f_lengthsqr(a), ~=, 25.0f);
    RC_CHECK(rc_vec2f_length(a), ~=, 5.0f);

    rc_vec2f n = rc_vec2f_normalize(a);
    RC_CHECK(n, ~=, rc_vec2f_make(0.6f, 0.8f));
    RC_CHECK(rc_vec2f_length(n), ~=, 1.0f);

    RC_CHECK(rc_vec2f_normalize_safe(a, 0.001f), ~=, n);
    // below tolerance returns zero
    RC_CHECK(rc_vec2f_normalize_safe(rc_vec2f_make_zero(), 0.001f), ==, rc_vec2f_make_zero());
}

RC_TEST(vec2f, equality)
{
    rc_vec2f a = rc_vec2f_make(1.0f, 1.0f);
    RC_CHECK(a, ~=, rc_vec2f_make(1.00001f, 1.0f));
    RC_CHECK(a, !=, rc_vec2f_make(2.0f, 1.0f));
    RC_CHECK(a, ==, rc_vec2f_make(1.0f, 1.0f));
    // is_nearly_equal uses Euclidean distance; exercise it directly too
    RC_CHECK_TRUE(rc_vec2f_is_nearly_equal(a, rc_vec2f_make(1.00001f, 1.0f), 0.001f));
    RC_CHECK_FALSE(rc_vec2f_is_nearly_equal(a, rc_vec2f_make(2.0f, 1.0f), 0.001f));
}
