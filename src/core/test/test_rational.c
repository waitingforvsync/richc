#include "richc/math/rational.h"
#include "richc/test.h"

RC_TEST(rational, make_canonical)
{
    // reduced to lowest terms
    RC_CHECK(rc_rational_make(4, 2), ==, rc_rational_make(2, 1));
    RC_CHECK(rc_rational_make(6, 9), ==, rc_rational_make(2, 3));
    // the sign is normalised onto the numerator
    RC_CHECK(rc_rational_make(6, -8), ==, rc_rational_make(-3, 4));
    RC_CHECK(rc_rational_make(-6, -8), ==, rc_rational_make(3, 4));
    RC_CHECK(rc_rational_make(-6, 8), ==, rc_rational_make(-3, 4));
    // zero canonicalises to 0/1
    RC_CHECK(rc_rational_make(0, 5), ==, rc_rational_from_i64(0));

    // division by zero is the invalid state 0/0 (read via the accessors, since
    // comparison of an invalid rational is not allowed)
    rc_rational bad = rc_rational_make(5, 0);
    RC_CHECK_FALSE(rc_rational_is_valid(bad));
    RC_CHECK(rc_rational_num(bad), ==, 0);
    RC_CHECK(rc_rational_denom(bad), ==, 0);
}

RC_TEST(rational, predicates)
{
    RC_CHECK_TRUE(rc_rational_is_valid(rc_rational_make(3, 4)));
    RC_CHECK_TRUE(rc_rational_is_zero(rc_rational_from_i64(0)));
    RC_CHECK_FALSE(rc_rational_is_zero(rc_rational_make(1, 2)));
    RC_CHECK_TRUE(rc_rational_is_integer(rc_rational_make(8, 4)));
    RC_CHECK_FALSE(rc_rational_is_integer(rc_rational_make(3, 2)));
    RC_CHECK_TRUE(rc_rational_is_positive(rc_rational_make(1, 2)));
    RC_CHECK_FALSE(rc_rational_is_positive(rc_rational_make(-1, 2)));
    RC_CHECK_TRUE(rc_rational_is_negative(rc_rational_make(-1, 2)));
    RC_CHECK_FALSE(rc_rational_is_negative(rc_rational_from_i64(0)));
}

RC_TEST(rational, from_i64)
{
    RC_CHECK(rc_rational_from_i64(7), ==, rc_rational_make(7, 1));
    RC_CHECK(rc_rational_from_i64(-5), ==, rc_rational_make(-5, 1));
}

RC_TEST(rational, from_double)
{
    RC_CHECK(rc_rational_from_double(0.5, 1e-9), ==, rc_rational_make(1, 2));
    RC_CHECK(rc_rational_from_double(0.1, 1e-9), ==, rc_rational_make(1, 10));
    RC_CHECK(rc_rational_from_double(-0.75, 1e-9), ==, rc_rational_make(-3, 4));
    // a loose threshold finds the simplest nearby rational
    RC_CHECK(rc_rational_from_double(0.3333333, 1e-4), ==, rc_rational_make(1, 3));
}

RC_TEST(rational, negate_abs_reciprocal)
{
    RC_CHECK(rc_rational_negate(rc_rational_make(3, 4)), ==, rc_rational_make(-3, 4));
    RC_CHECK(rc_rational_negate(rc_rational_make(-3, 4)), ==, rc_rational_make(3, 4));
    RC_CHECK(rc_rational_abs(rc_rational_make(-3, 4)), ==, rc_rational_make(3, 4));
    RC_CHECK(rc_rational_abs(rc_rational_make(3, 4)), ==, rc_rational_make(3, 4));
    // reciprocal moves the sign back onto the numerator
    RC_CHECK(rc_rational_reciprocal(rc_rational_make(3, 4)), ==, rc_rational_make(4, 3));
    RC_CHECK(rc_rational_reciprocal(rc_rational_make(-3, 4)), ==, rc_rational_make(-4, 3));
}

RC_TEST(rational, multiply)
{
    // 2/3 * 3/4 = 1/2 (cross-GCD keeps it in lowest terms)
    RC_CHECK(rc_rational_mul(rc_rational_make(2, 3), rc_rational_make(3, 4)), ==, rc_rational_make(1, 2));
    RC_CHECK(rc_rational_mul(rc_rational_make(-2, 3), rc_rational_make(3, 4)), ==, rc_rational_make(-1, 2));
    RC_CHECK(rc_rational_int_mul(rc_rational_make(3, 4), 2), ==, rc_rational_make(3, 2));
    RC_CHECK(rc_rational_int_mul(rc_rational_make(3, 4), 0), ==, rc_rational_from_i64(0));
}

RC_TEST(rational, divide)
{
    // 2/3 / 4/5 = 5/6
    RC_CHECK(rc_rational_div(rc_rational_make(2, 3), rc_rational_make(4, 5)), ==, rc_rational_make(5, 6));
    // dividing by a negative normalises the sign
    RC_CHECK(rc_rational_div(rc_rational_make(2, 3), rc_rational_make(-4, 5)), ==, rc_rational_make(-5, 6));
    RC_CHECK(rc_rational_int_div(rc_rational_make(3, 4), 2), ==, rc_rational_make(3, 8));
    RC_CHECK(rc_rational_int_div(rc_rational_make(3, 4), -2), ==, rc_rational_make(-3, 8));
}

RC_TEST(rational, add_sub)
{
    // 1/2 + 1/3 = 5/6, 1/2 - 1/3 = 1/6
    RC_CHECK(rc_rational_add(rc_rational_make(1, 2), rc_rational_make(1, 3)), ==, rc_rational_make(5, 6));
    RC_CHECK(rc_rational_sub(rc_rational_make(1, 2), rc_rational_make(1, 3)), ==, rc_rational_make(1, 6));
    // results land in lowest terms: 1/6 + 1/6 = 1/3
    RC_CHECK(rc_rational_add(rc_rational_make(1, 6), rc_rational_make(1, 6)), ==, rc_rational_make(1, 3));
    // 1/2 - 1/2 = 0/1
    RC_CHECK(rc_rational_sub(rc_rational_make(1, 2), rc_rational_make(1, 2)), ==, rc_rational_from_i64(0));
    RC_CHECK(rc_rational_int_add(rc_rational_make(3, 4), 2), ==, rc_rational_make(11, 4));
    RC_CHECK(rc_rational_int_sub(rc_rational_make(3, 4), 2), ==, rc_rational_make(-5, 4));
}

RC_TEST(rational, compare)
{
    rc_rational half = rc_rational_make(1, 2);
    rc_rational third = rc_rational_make(1, 3);
    RC_CHECK(half, >, third);
    RC_CHECK(third, <, half);
    RC_CHECK(half, ==, rc_rational_make(2, 4));
    RC_CHECK(half, !=, third);
    RC_CHECK(half, >=, rc_rational_make(2, 4));
    RC_CHECK(third, <=, half);
    RC_CHECK(rc_rational_min(half, third), ==, third);
    RC_CHECK(rc_rational_max(half, third), ==, half);

    // negatives order correctly: -3/4 < -2/3 < 1/3
    RC_CHECK(rc_rational_make(-3, 4), <, rc_rational_make(-2, 3));
    RC_CHECK(rc_rational_make(-1, 2), <, rc_rational_make(1, 3));
}

RC_TEST(rational, compare_no_overflow)
{
    // same denominator, numerators near INT64_MAX: the cross product
    // a.num * b.denom overflows int64, but the continued-fraction compare does not
    rc_rational a = rc_rational_make(INT64_MAX, 5);
    rc_rational b = rc_rational_make(INT64_MAX - 1, 5);
    RC_CHECK(a, >, b);
    RC_CHECK(b, <, a);

    // different denominators; the cross product (~4.9e19) overflows int64
    rc_rational c = rc_rational_make(7000000000000000000, 3);   // ~2.33e18
    rc_rational d = rc_rational_make(7000000000000000001, 7);   // ~1.0e18
    RC_CHECK(c, >, d);
    RC_CHECK(d, <, c);
    // a value compares equal to itself even when huge
    RC_CHECK(c, ==, c);
}

RC_TEST(rational, to_double)
{
    RC_CHECK(rc_rational_to_double(rc_rational_make(1, 2)), ~=, 0.5);
    RC_CHECK(rc_rational_to_double(rc_rational_make(-3, 4)), ~=, -0.75);
    RC_CHECK(rc_rational_to_double(rc_rational_from_i64(7)), ~=, 7.0);
}
