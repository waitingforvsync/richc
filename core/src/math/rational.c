/*
 * math/rational.c - non-trivial rc_rational operations.
 *
 * All functions assert that their inputs are valid (rc_rational_is_valid) and
 * that arithmetic results do not overflow int64_t, using rc_mul_overflows_i64,
 * rc_add_overflows_i64, and rc_sub_overflows_i64 from ops.h.
 *
 * Algorithm notes
 * ---------------
 * mul / int_mul: GCD pre-reduction before multiplying keeps intermediate values
 *   small and yields a result already in canonical form (no further GCD needed).
 *
 * div / int_div: same cross-GCD strategy as mul (division is multiply by the
 *   reciprocal); sign normalisation is handled with an explicit flip.
 *
 * add / sub: AHU algorithm -
 *   d = gcd(a.denom, b.denom)
 *   t = a.num * (b.denom/d)  +-  b.num * (a.denom/d)
 *   g = gcd(t, d)
 *   result = { t/g, (a.denom/d) * (b.denom/g) }
 *   This avoids taking a GCD against the full LCM and yields a result already in
 *   canonical form (all four required coprimality conditions follow from a, b
 *   being canonical and the properties of GCD).
 *
 * compare: continued-fraction descent that never multiplies, so it cannot
 *   overflow even when a cross product would.
 *
 * from_double: continued-fraction algorithm.  Convergents p_k/q_k are the best
 *   rational approximations (no fraction with a smaller denominator lies closer
 *   to val), so the first one inside the tolerance band is the simplest rational
 *   satisfying |val - p/q| <= threshold.
 */

#include "richc/math/rational.h"

#include <math.h>   // isfinite, fabs

/*
 * Reduce num/denom to canonical form: denom > 0 (signs flipped if needed) with
 * both divided by their GCD.  Returns the invalid 0/0 when denom == 0.  Asserts
 * that sign-flipping num or denom does not overflow int64_t.
 */
rc_rational rc_rational_make(int64_t num, int64_t denom)
{
    if (denom == 0) {
        return (rc_rational) {0};
    }
    if (denom < 0) {
        RC_ASSERT(num   != INT64_MIN);
        RC_ASSERT(denom != INT64_MIN);
        num = -num;
        denom = -denom;
    }
    int64_t d = rc_gcd_i64(num, denom);
    return (rc_rational) {.num_ = num / d, .denom_ = denom / d};
}

/*
 * Find the simplest rational within threshold of val.  Steps through continued-
 * fraction convergents p_k/q_k until one satisfies |val - p_k/q_k| <= threshold,
 * the expansion terminates (fractional part drops below 1e-10, meaning val is
 * rational at double precision), or the next convergent would overflow int64_t.
 */
rc_rational rc_rational_from_double(double val, double threshold)
{
    RC_ASSERT(isfinite(val));
    RC_ASSERT(threshold >= 0.0);

    bool   negative = val < 0.0;
    double x        = negative ? -val : val;
    RC_ASSERT(x < (double)INT64_MAX);

    // first convergent: the integer part of x
    int64_t a    = (int64_t)x;   // truncation == floor for x >= 0
    double  frac = x - (double)a;
    int64_t p_prv = 1, p_cur = a;
    int64_t q_prv = 0, q_cur = 1;

    while (fabs(x - (double)p_cur / (double)q_cur) > threshold) {
        // fractional part too small to invert reliably: the expansion ends
        if (frac < 1e-10) {
            break;
        }
        double recip = 1.0 / frac;
        a    = (int64_t)recip;   // truncation == floor for recip > 0
        frac = recip - (double)a;

        // stop before a convergent that would overflow int64_t
        if ((p_cur > 0 && a > (INT64_MAX - p_prv) / p_cur) ||
            (q_cur > 0 && a > (INT64_MAX - q_prv) / q_cur)) {
            break;
        }
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
    int64_t g  = rc_gcd_i64(a.denom_, b);
    int64_t bq = b / g;
    RC_ASSERT(!rc_mul_overflows_i64(a.num_, bq));
    return (rc_rational) {.num_ = a.num_ * bq, .denom_ = a.denom_ / g};
}

/*
 * a * b.
 * Cross-GCD pre-reduction: g1 = gcd(a.num, b.denom), g2 = gcd(b.num, a.denom).
 */
rc_rational rc_rational_mul(rc_rational a, rc_rational b)
{
    RC_ASSERT(rc_rational_is_valid(a));
    RC_ASSERT(rc_rational_is_valid(b));
    int64_t g1 = rc_gcd_i64(a.num_,   b.denom_);
    int64_t g2 = rc_gcd_i64(b.num_,   a.denom_);
    int64_t an = a.num_   / g1;
    int64_t bn = b.num_   / g2;
    int64_t ad = a.denom_ / g2;
    int64_t bd = b.denom_ / g1;
    RC_ASSERT(!rc_mul_overflows_i64(an, bn));
    RC_ASSERT(!rc_mul_overflows_i64(ad, bd));
    return (rc_rational) {.num_ = an * bn, .denom_ = ad * bd};
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
    int64_t g  = rc_gcd_i64(a.num_, b);
    int64_t n  = a.num_ / g;
    int64_t bq = b / g;
    RC_ASSERT(!rc_mul_overflows_i64(a.denom_, bq));
    int64_t d = a.denom_ * bq;
    if (d < 0) {
        RC_ASSERT(n != INT64_MIN);
        n = -n;
        d = -d;
    }
    return (rc_rational) {.num_ = n, .denom_ = d};
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
    RC_ASSERT(b.num_ != 0);
    int64_t g1 = rc_gcd_i64(a.num_,   b.num_);
    int64_t g2 = rc_gcd_i64(a.denom_, b.denom_);
    int64_t an = a.num_   / g1;
    int64_t bd = b.denom_ / g2;
    int64_t ad = a.denom_ / g2;
    int64_t bn = b.num_   / g1;
    RC_ASSERT(!rc_mul_overflows_i64(an, bd));
    RC_ASSERT(!rc_mul_overflows_i64(ad, bn));
    int64_t n = an * bd;
    int64_t d = ad * bn;
    if (d < 0) {
        RC_ASSERT(n != INT64_MIN);
        n = -n;
        d = -d;
    }
    return (rc_rational) {.num_ = n, .denom_ = d};
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
    int64_t d  = rc_gcd_i64(a.denom_, b.denom_);
    int64_t db = b.denom_ / d;
    int64_t da = a.denom_ / d;
    RC_ASSERT(!rc_mul_overflows_i64(a.num_, db));
    RC_ASSERT(!rc_mul_overflows_i64(b.num_, da));
    int64_t anum_db = a.num_ * db;
    int64_t bnum_da = b.num_ * da;
    RC_ASSERT(!rc_add_overflows_i64(anum_db, bnum_da));
    int64_t t = anum_db + bnum_da;
    int64_t g = rc_gcd_i64(t, d);
    RC_ASSERT(!rc_mul_overflows_i64(a.denom_ / d, b.denom_ / g));
    return (rc_rational) {.num_ = t / g, .denom_ = (a.denom_ / d) * (b.denom_ / g)};
}

/*
 * a - b.  Same AHU algorithm as add with subtraction in the numerator.
 */
rc_rational rc_rational_sub(rc_rational a, rc_rational b)
{
    RC_ASSERT(rc_rational_is_valid(a));
    RC_ASSERT(rc_rational_is_valid(b));
    int64_t d  = rc_gcd_i64(a.denom_, b.denom_);
    int64_t db = b.denom_ / d;
    int64_t da = a.denom_ / d;
    RC_ASSERT(!rc_mul_overflows_i64(a.num_, db));
    RC_ASSERT(!rc_mul_overflows_i64(b.num_, da));
    int64_t anum_db = a.num_ * db;
    int64_t bnum_da = b.num_ * da;
    RC_ASSERT(!rc_sub_overflows_i64(anum_db, bnum_da));
    int64_t t = anum_db - bnum_da;
    int64_t g = rc_gcd_i64(t, d);
    RC_ASSERT(!rc_mul_overflows_i64(a.denom_ / d, b.denom_ / g));
    return (rc_rational) {.num_ = t / g, .denom_ = (a.denom_ / d) * (b.denom_ / g)};
}

/*
 * Compare a and b without ever multiplying, so it cannot overflow even when the
 * cross product a.num*b.denom would.
 *
 * Walk the continued-fraction expansions: compare the floor parts, and when they
 * agree, recurse on the reciprocals of the fractional remainders - which flips
 * the order, tracked by `sign`.  Only floor division and remainder are used, and
 * the remainders strictly shrink (the subtractive Euclidean descent), so it
 * terminates.  The remainders are taken with floor semantics (in [0, denom)) so
 * negative operands are ordered correctly.
 */
int32_t rc_rational_compare(rc_rational a, rc_rational b)
{
    RC_ASSERT(rc_rational_is_valid(a));
    RC_ASSERT(rc_rational_is_valid(b));

    int64_t p = a.num_, q = a.denom_;
    int64_t r = b.num_, s = b.denom_;
    int32_t sign = 1;

    for (;;) {
        // floor(p/q) and remainder in [0, q); q and s stay positive throughout
        int64_t pf = p / q, pm = p % q;
        if (pm < 0) { pf -= 1; pm += q; }
        int64_t rf = r / s, rm = r % s;
        if (rm < 0) { rf -= 1; rm += s; }

        if (pf != rf) {
            return (pf < rf) ? -sign : sign;
        }
        if (pm == 0 || rm == 0) {
            if (pm == 0 && rm == 0) {
                return 0;
            }
            // the value with the zero fractional part is the smaller one
            return (pm == 0) ? -sign : sign;
        }
        // both fractional parts lie in (0, 1); compare their reciprocals, which
        // reverses the ordering
        p = q; q = pm;
        r = s; s = rm;
        sign = -sign;
    }
}
