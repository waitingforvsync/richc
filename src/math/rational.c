/*
 * math/rational.c - non-trivial rc_rational operations.
 *
 * All functions assert that their inputs are valid (rc_rational_is_valid) and
 * that arithmetic results do not overflow int64_t, using rc_mul_overflows_i64,
 * rc_add_overflows_i64, and rc_sub_overflows_i64 from math/math.h.
 *
 * Algorithm notes
 * ---------------
 * mul / int_mul: GCD pre-reduction before multiplying keeps intermediate values
 *   small and yields a result already in canonical form (no further GCD needed).
 *
 * div / int_div: same cross-GCD strategy as mul (division is multiply by the
 *   reciprocal); sign normalisation is handled with an explicit flip.
 *
 * add / sub: AHU algorithm —
 *   d = gcd(a.denom, b.denom)
 *   t = a.num * (b.denom/d)  ±  b.num * (a.denom/d)
 *   g = gcd(t, d)
 *   result = { t/g, (a.denom/d) * (b.denom/g) }
 *   This avoids taking a GCD against the full LCM and yields a result already
 *   in canonical form (proof: all four required coprimality conditions follow
 *   from a, b being canonical and the properties of GCD).
 *
 * from_double: continued-fraction algorithm.  Convergents p_k/q_k are the
 *   best rational approximations (no fraction with a smaller denominator lies
 *   closer to val), so the first one inside the tolerance band is the simplest
 *   rational satisfying |val - p/q| <= threshold.
 */

#include <math.h>   /* isfinite, fabs */
#include "richc/math/rational.h"

/*
 * Reduce num/denom to canonical form.
 * Ensures denom > 0 (flips signs if needed) and divides both by their GCD.
 * Returns {0, 0} (invalid) if denom == 0.
 * Asserts that sign-flipping num or denom does not overflow int64_t.
 */
rc_rational rc_rational_make(int64_t num, int64_t denom)
{
    if (denom == 0) return (rc_rational) {0, 0};
    if (denom < 0) {
        RC_ASSERT(num   != INT64_MIN);
        RC_ASSERT(denom != INT64_MIN);
        num = -num; denom = -denom;
    }
    int64_t d = rc_gcd_i64(num, denom);
    return (rc_rational) {num / d, denom / d};
}

/*
 * Find the simplest rational within threshold of val.
 * Steps through continued-fraction convergents p_k/q_k until one satisfies
 * |val - p_k/q_k| <= threshold, the expansion terminates (fractional part
 * drops below 1e-10, meaning val is rational at double precision), or the
 * next convergent would overflow int64_t.
 */
rc_rational rc_rational_from_double(double val, double threshold)
{
    RC_ASSERT(isfinite(val));
    RC_ASSERT(threshold >= 0.0);

    bool   negative = val < 0.0;
    double x        = negative ? -val : val;
    RC_ASSERT(x < (double)INT64_MAX);

    /* First convergent: integer part of x. */
    int64_t a    = (int64_t)x;   /* truncation == floor for x >= 0 */
    double  frac = x - (double)a;
    int64_t p_prv = 1, p_cur = a;
    int64_t q_prv = 0, q_cur = 1;

    while (fabs(x - (double)p_cur / (double)q_cur) > threshold) {
        /* Fractional part too small to invert reliably: CF terminates. */
        if (frac < 1e-10) break;

        double  recip = 1.0 / frac;
        a             = (int64_t)recip;   /* truncation == floor for recip > 0 */
        frac          = recip - (double)a;

        /* Stop before a convergent that would overflow int64_t. */
        if ((p_cur > 0 && a > (INT64_MAX - p_prv) / p_cur) ||
            (q_cur > 0 && a > (INT64_MAX - q_prv) / q_cur))
            break;

        int64_t p_nxt = a * p_cur + p_prv;
        int64_t q_nxt = a * q_cur + q_prv;
        p_prv = p_cur; p_cur = p_nxt;
        q_prv = q_cur; q_cur = q_nxt;
    }

    return rc_rational_make(negative ? -p_cur : p_cur, q_cur);
}

/*
 * a * b  (b integer).
 * Pre-reduces via g = gcd(a.denom, b): result = { a.num * (b/g), a.denom/g }.
 */
rc_rational rc_rational_int_mul(rc_rational a, int64_t b)
{
    RC_ASSERT(rc_rational_is_valid(a));
    int64_t g  = rc_gcd_i64(a.denom, b);
    int64_t bq = b / g;
    RC_ASSERT(!rc_mul_overflows_i64(a.num, bq));
    return (rc_rational) {a.num * bq, a.denom / g};
}

/*
 * a * b.
 * Cross-GCD pre-reduction: g1 = gcd(a.num, b.denom), g2 = gcd(b.num, a.denom).
 */
rc_rational rc_rational_mul(rc_rational a, rc_rational b)
{
    RC_ASSERT(rc_rational_is_valid(a));
    RC_ASSERT(rc_rational_is_valid(b));
    int64_t g1 = rc_gcd_i64(a.num,   b.denom);
    int64_t g2 = rc_gcd_i64(b.num,   a.denom);
    int64_t an = a.num   / g1;
    int64_t bn = b.num   / g2;
    int64_t ad = a.denom / g2;
    int64_t bd = b.denom / g1;
    RC_ASSERT(!rc_mul_overflows_i64(an, bn));
    RC_ASSERT(!rc_mul_overflows_i64(ad, bd));
    return (rc_rational) {an * bn, ad * bd};
}

/*
 * a / b  (b non-zero integer).
 * Pre-reduces via g = gcd(a.num, b): result = { a.num/g, a.denom * (b/g) }.
 * Sign normalisation is handled with an explicit flip when b/g < 0.
 */
rc_rational rc_rational_int_div(rc_rational a, int64_t b)
{
    RC_ASSERT(rc_rational_is_valid(a));
    RC_ASSERT(b != 0);
    int64_t g  = rc_gcd_i64(a.num, b);
    int64_t n  = a.num / g;
    int64_t bq = b / g;
    RC_ASSERT(!rc_mul_overflows_i64(a.denom, bq));
    int64_t d = a.denom * bq;
    if (d < 0) { RC_ASSERT(n != INT64_MIN); n = -n; d = -d; }
    return (rc_rational) {n, d};
}

/*
 * a / b  (b non-zero rational).
 * Cross-GCD pre-reduction: g1 = gcd(a.num, b.num), g2 = gcd(a.denom, b.denom).
 * Sign normalisation is handled with an explicit flip when b.num/g1 < 0.
 */
rc_rational rc_rational_div(rc_rational a, rc_rational b)
{
    RC_ASSERT(rc_rational_is_valid(a));
    RC_ASSERT(rc_rational_is_valid(b));
    RC_ASSERT(b.num != 0);
    int64_t g1 = rc_gcd_i64(a.num,   b.num);
    int64_t g2 = rc_gcd_i64(a.denom, b.denom);
    int64_t an = a.num   / g1;
    int64_t bd = b.denom / g2;
    int64_t ad = a.denom / g2;
    int64_t bn = b.num   / g1;
    RC_ASSERT(!rc_mul_overflows_i64(an, bd));
    RC_ASSERT(!rc_mul_overflows_i64(ad, bn));
    int64_t n = an * bd;
    int64_t d = ad * bn;
    if (d < 0) { RC_ASSERT(n != INT64_MIN); n = -n; d = -d; }
    return (rc_rational) {n, d};
}

/*
 * a + b.  AHU algorithm:
 *   d = gcd(a.denom, b.denom)
 *   t = a.num*(b.denom/d) + b.num*(a.denom/d)
 *   g = gcd(t, d)
 *   result = { t/g, (a.denom/d) * (b.denom/g) }
 */
rc_rational rc_rational_add(rc_rational a, rc_rational b)
{
    RC_ASSERT(rc_rational_is_valid(a));
    RC_ASSERT(rc_rational_is_valid(b));
    int64_t d       = rc_gcd_i64(a.denom, b.denom);
    int64_t db      = b.denom / d;
    int64_t da      = a.denom / d;
    RC_ASSERT(!rc_mul_overflows_i64(a.num, db));
    RC_ASSERT(!rc_mul_overflows_i64(b.num, da));
    int64_t anum_db = a.num * db;
    int64_t bnum_da = b.num * da;
    RC_ASSERT(!rc_add_overflows_i64(anum_db, bnum_da));
    int64_t t = anum_db + bnum_da;
    int64_t g = rc_gcd_i64(t, d);
    RC_ASSERT(!rc_mul_overflows_i64(a.denom / d, b.denom / g));
    return (rc_rational) {t / g, (a.denom / d) * (b.denom / g)};
}

/*
 * a - b.  Same AHU algorithm as add with subtraction in the numerator.
 */
rc_rational rc_rational_sub(rc_rational a, rc_rational b)
{
    RC_ASSERT(rc_rational_is_valid(a));
    RC_ASSERT(rc_rational_is_valid(b));
    int64_t d       = rc_gcd_i64(a.denom, b.denom);
    int64_t db      = b.denom / d;
    int64_t da      = a.denom / d;
    RC_ASSERT(!rc_mul_overflows_i64(a.num, db));
    RC_ASSERT(!rc_mul_overflows_i64(b.num, da));
    int64_t anum_db = a.num * db;
    int64_t bnum_da = b.num * da;
    RC_ASSERT(!rc_sub_overflows_i64(anum_db, bnum_da));
    int64_t t = anum_db - bnum_da;
    int64_t g = rc_gcd_i64(t, d);
    RC_ASSERT(!rc_mul_overflows_i64(a.denom / d, b.denom / g));
    return (rc_rational) {t / g, (a.denom / d) * (b.denom / g)};
}
