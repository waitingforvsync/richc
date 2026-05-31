#include "richc/math/solve.h"
#include "richc/test.h"

// Is `value` among the first n roots (within a small tolerance)?
static bool has_root(const float *root, int n, float value)
{
    for (int i = 0; i < n; i++) {
        float d = root[i] - value;
        if (d > -1e-4f && d < 1e-4f) {
            return true;
        }
    }
    return false;
}

// Evaluate the polynomials by Horner's rule, for residual checks.
static float quad_at(float a, float b, float c, float t)
{
    return (a * t + b) * t + c;
}

static float cubic_at(float a, float b, float c, float d, float t)
{
    return ((a * t + b) * t + c) * t + d;
}

RC_TEST(solve, quadratic_two_roots)
{
    // (t - 2)(t - 3) = t^2 - 5t + 6
    rc_quadratic_roots r = rc_solve_quadratic(1.0f, -5.0f, 6.0f);
    RC_CHECK(r.num_roots, ==, 2);
    RC_CHECK_TRUE(has_root(r.root, 2, 2.0f));
    RC_CHECK_TRUE(has_root(r.root, 2, 3.0f));
    RC_CHECK(quad_at(1.0f, -5.0f, 6.0f, r.root[0]), ~=, 0.0f);
    RC_CHECK(quad_at(1.0f, -5.0f, 6.0f, r.root[1]), ~=, 0.0f);
}

RC_TEST(solve, quadratic_double_root)
{
    // (t - 4)^2 = t^2 - 8t + 16
    rc_quadratic_roots r = rc_solve_quadratic(1.0f, -8.0f, 16.0f);
    RC_CHECK(r.num_roots, ==, 1);
    RC_CHECK(r.root[0], ~=, 4.0f);
}

RC_TEST(solve, quadratic_no_roots)
{
    // t^2 + 1 has no real roots
    rc_quadratic_roots r = rc_solve_quadratic(1.0f, 0.0f, 1.0f);
    RC_CHECK(r.num_roots, ==, 0);
}

RC_TEST(solve, quadratic_linear)
{
    // a == 0 falls back to linear: 2t - 6 = 0
    rc_quadratic_roots r = rc_solve_quadratic(0.0f, 2.0f, -6.0f);
    RC_CHECK(r.num_roots, ==, 1);
    RC_CHECK(r.root[0], ~=, 3.0f);

    // a == 0 and b == 0: no solution
    rc_quadratic_roots z = rc_solve_quadratic(0.0f, 0.0f, 5.0f);
    RC_CHECK(z.num_roots, ==, 0);
}

RC_TEST(solve, quadratic_stable)
{
    // roots near -1e8 and -1e-8.  The naive (-b + sqrt(disc)) / 2a collapses the
    // small root to zero; the sign-selected method preserves it, so the product
    // of the roots stays c/a = 1.
    rc_quadratic_roots r = rc_solve_quadratic(1.0f, 1.0e8f, 1.0f);
    RC_CHECK(r.num_roots, ==, 2);
    RC_CHECK(r.root[0] * r.root[1], ~=, 1.0f);
}

RC_TEST(solve, cubic_three_roots)
{
    // (t - 1)(t - 2)(t - 3) = t^3 - 6t^2 + 11t - 6
    rc_cubic_roots r = rc_solve_cubic(1.0f, -6.0f, 11.0f, -6.0f);
    RC_CHECK(r.num_roots, ==, 3);
    RC_CHECK_TRUE(has_root(r.root, 3, 1.0f));
    RC_CHECK_TRUE(has_root(r.root, 3, 2.0f));
    RC_CHECK_TRUE(has_root(r.root, 3, 3.0f));
    RC_CHECK(cubic_at(1.0f, -6.0f, 11.0f, -6.0f, r.root[0]), ~=, 0.0f);
    RC_CHECK(cubic_at(1.0f, -6.0f, 11.0f, -6.0f, r.root[1]), ~=, 0.0f);
    RC_CHECK(cubic_at(1.0f, -6.0f, 11.0f, -6.0f, r.root[2]), ~=, 0.0f);
}

RC_TEST(solve, cubic_one_root)
{
    // (t - 2)(t^2 + 1) = t^3 - 2t^2 + t - 2 ; only t = 2 is real
    rc_cubic_roots r = rc_solve_cubic(1.0f, -2.0f, 1.0f, -2.0f);
    RC_CHECK(r.num_roots, ==, 1);
    RC_CHECK(r.root[0], ~=, 2.0f);
}

RC_TEST(solve, cubic_triple_root)
{
    // (t - 1)^3 = t^3 - 3t^2 + 3t - 1
    rc_cubic_roots r = rc_solve_cubic(1.0f, -3.0f, 3.0f, -1.0f);
    RC_CHECK(r.num_roots, ==, 1);
    RC_CHECK(r.root[0], ~=, 1.0f);
}

RC_TEST(solve, cubic_repeated_root)
{
    // (t - 1)^2 (t - 4) = t^3 - 6t^2 + 9t - 4 ; three real roots, two coincident
    rc_cubic_roots r = rc_solve_cubic(1.0f, -6.0f, 9.0f, -4.0f);
    RC_CHECK(r.num_roots, ==, 3);
    RC_CHECK_TRUE(has_root(r.root, 3, 1.0f));
    RC_CHECK_TRUE(has_root(r.root, 3, 4.0f));
    RC_CHECK(cubic_at(1.0f, -6.0f, 9.0f, -4.0f, r.root[0]), ~=, 0.0f);
    RC_CHECK(cubic_at(1.0f, -6.0f, 9.0f, -4.0f, r.root[1]), ~=, 0.0f);
    RC_CHECK(cubic_at(1.0f, -6.0f, 9.0f, -4.0f, r.root[2]), ~=, 0.0f);
}

RC_TEST(solve, cubic_degenerate)
{
    // a == 0 delegates to the quadratic: t^2 - 5t + 6
    rc_cubic_roots r = rc_solve_cubic(0.0f, 1.0f, -5.0f, 6.0f);
    RC_CHECK(r.num_roots, ==, 2);
    RC_CHECK_TRUE(has_root(r.root, 2, 2.0f));
    RC_CHECK_TRUE(has_root(r.root, 2, 3.0f));
}
