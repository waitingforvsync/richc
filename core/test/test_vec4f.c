#include "richc/math/vec4f.h"
#include "richc/test.h"

RC_TEST(vec4f, construct)
{
    rc_vec4f a = rc_vec4f_make(1.0f, 2.0f, 3.0f, 4.0f);
    RC_CHECK(a, ==, rc_vec4f_make(1.0f, 2.0f, 3.0f, 4.0f));
    RC_CHECK(rc_vec4f_make_zero(), ==, rc_vec4f_make(0.0f, 0.0f, 0.0f, 0.0f));
    RC_CHECK(rc_vec4f_make_unitx(), ==, rc_vec4f_make(1.0f, 0.0f, 0.0f, 0.0f));
    RC_CHECK(rc_vec4f_make_unity(), ==, rc_vec4f_make(0.0f, 1.0f, 0.0f, 0.0f));
    RC_CHECK(rc_vec4f_make_unitz(), ==, rc_vec4f_make(0.0f, 0.0f, 1.0f, 0.0f));
    RC_CHECK(rc_vec4f_make_unitw(), ==, rc_vec4f_make(0.0f, 0.0f, 0.0f, 1.0f));

    float raw[] = {5.0f, 6.0f, 7.0f, 8.0f};
    RC_CHECK(rc_vec4f_from_floats(raw), ==, rc_vec4f_make(5.0f, 6.0f, 7.0f, 8.0f));
    RC_CHECK(rc_vec4f_from_vec2f(rc_vec2f_make(1.0f, 2.0f), 3.0f, 4.0f), ==, rc_vec4f_make(1.0f, 2.0f, 3.0f, 4.0f));
    RC_CHECK(rc_vec4f_from_vec3f(rc_vec3f_make(1.0f, 2.0f, 3.0f), 4.0f), ==, rc_vec4f_make(1.0f, 2.0f, 3.0f, 4.0f));
    RC_CHECK_TRUE(rc_vec4f_as_floats(&a) == &a.x);
}

RC_TEST(vec4f, arithmetic)
{
    rc_vec4f a = rc_vec4f_make(1.0f, 2.0f, 3.0f, 4.0f);
    rc_vec4f b = rc_vec4f_make(10.0f, 20.0f, 30.0f, 40.0f);
    RC_CHECK(rc_vec4f_add(a, b), ==, rc_vec4f_make(11.0f, 22.0f, 33.0f, 44.0f));
    RC_CHECK(rc_vec4f_add3(a, b, a), ==, rc_vec4f_make(12.0f, 24.0f, 36.0f, 48.0f));
    RC_CHECK(rc_vec4f_add4(a, b, a, b), ==, rc_vec4f_make(22.0f, 44.0f, 66.0f, 88.0f));
    RC_CHECK(rc_vec4f_sub(b, a), ==, rc_vec4f_make(9.0f, 18.0f, 27.0f, 36.0f));
    RC_CHECK(rc_vec4f_scalar_mul(a, 3.0f), ==, rc_vec4f_make(3.0f, 6.0f, 9.0f, 12.0f));
    RC_CHECK(rc_vec4f_scalar_div(b, 10.0f), ==, rc_vec4f_make(1.0f, 2.0f, 3.0f, 4.0f));
    RC_CHECK(rc_vec4f_component_mul(a, b), ==, rc_vec4f_make(10.0f, 40.0f, 90.0f, 160.0f));
    RC_CHECK(rc_vec4f_negate(a), ==, rc_vec4f_make(-1.0f, -2.0f, -3.0f, -4.0f));
}

RC_TEST(vec4f, component_ops)
{
    rc_vec4f a = rc_vec4f_make(1.0f, 8.0f, 4.0f, 2.0f);
    rc_vec4f b = rc_vec4f_make(5.0f, 3.0f, 6.0f, 9.0f);
    RC_CHECK(rc_vec4f_component_min(a, b), ==, rc_vec4f_make(1.0f, 3.0f, 4.0f, 2.0f));
    RC_CHECK(rc_vec4f_component_max(a, b), ==, rc_vec4f_make(5.0f, 8.0f, 6.0f, 9.0f));

    rc_vec4f c = rc_vec4f_make(1.7f, -1.2f, 3.0f, -2.5f);
    RC_CHECK(rc_vec4f_component_floor(c), ==, rc_vec4f_make(1.0f, -2.0f, 3.0f, -3.0f));
    RC_CHECK(rc_vec4f_component_ceil(c), ==, rc_vec4f_make(2.0f, -1.0f, 3.0f, -2.0f));
    RC_CHECK(rc_vec4f_component_abs(rc_vec4f_make(-1.5f, 2.5f, -3.0f, 4.0f)), ==, rc_vec4f_make(1.5f, 2.5f, 3.0f, 4.0f));
}

RC_TEST(vec4f, lerp)
{
    rc_vec4f a = rc_vec4f_make(0.0f, 0.0f, 0.0f, 0.0f);
    rc_vec4f b = rc_vec4f_make(10.0f, 20.0f, 40.0f, 80.0f);
    RC_CHECK(rc_vec4f_lerp(a, b, 0.0f), ==, a);
    RC_CHECK(rc_vec4f_lerp(a, b, 1.0f), ==, b);
    RC_CHECK(rc_vec4f_lerp(a, b, 0.25f), ~=, rc_vec4f_make(2.5f, 5.0f, 10.0f, 20.0f));
}

RC_TEST(vec4f, dot)
{
    rc_vec4f a = rc_vec4f_make(1.0f, 2.0f, 3.0f, 4.0f);
    rc_vec4f b = rc_vec4f_make(5.0f, 6.0f, 7.0f, 8.0f);
    RC_CHECK(rc_vec4f_dot(a, b), ~=, 70.0f);   // 1*5 + 2*6 + 3*7 + 4*8
}

RC_TEST(vec4f, length_normalize)
{
    rc_vec4f a = rc_vec4f_make(1.0f, 2.0f, 2.0f, 4.0f);   // length 5: 1 + 4 + 4 + 16 = 25
    RC_CHECK(rc_vec4f_lengthsqr(a), ~=, 25.0f);
    RC_CHECK(rc_vec4f_length(a), ~=, 5.0f);

    rc_vec4f n = rc_vec4f_normalize(a);
    RC_CHECK(n, ~=, rc_vec4f_make(0.2f, 0.4f, 0.4f, 0.8f));
    RC_CHECK(rc_vec4f_length(n), ~=, 1.0f);

    RC_CHECK(rc_vec4f_normalize_safe(a, 0.001f), ~=, n);
    RC_CHECK(rc_vec4f_normalize_safe(rc_vec4f_make_zero(), 0.001f), ==, rc_vec4f_make_zero());
}

RC_TEST(vec4f, equality)
{
    rc_vec4f a = rc_vec4f_make(1.0f, 1.0f, 1.0f, 1.0f);
    RC_CHECK(a, ~=, rc_vec4f_make(1.00001f, 1.0f, 1.0f, 1.0f));
    RC_CHECK(a, !=, rc_vec4f_make(2.0f, 1.0f, 1.0f, 1.0f));
    RC_CHECK(a, ==, rc_vec4f_make(1.0f, 1.0f, 1.0f, 1.0f));
    RC_CHECK_TRUE(rc_vec4f_is_nearly_equal(a, rc_vec4f_make(1.00001f, 1.0f, 1.0f, 1.0f), 0.001f));
    RC_CHECK_FALSE(rc_vec4f_is_nearly_equal(a, rc_vec4f_make(2.0f, 1.0f, 1.0f, 1.0f), 0.001f));
}
