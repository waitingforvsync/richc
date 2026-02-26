/*
 * math/rational.h - rational number arithmetic (rc_rational).
 *
 * Rationals are stored in canonical form: denominator > 0, GCD(|num|, denom) = 1.
 * The zero rational has denom = 1.  Division by zero yields { 0, 0 } (invalid state;
 * rc_rational_is_valid returns false).
 *
 * Type
 * ----
 *   rc_rational  { int64_t num; int64_t denom; }
 *
 * States
 * ------
 *   Valid   : denom > 0, gcd(|num|, denom) == 1
 *   Invalid : denom == 0  (result of division by zero)
 *
 * Construction
 * ------------
 *   rc_rational_make(num, denom) — canonical form; denom==0 → invalid
 *   rc_rational_from_i64(n)     — integer n/1
 *
 * Predicates
 * ----------
 *   rc_rational_is_valid(a)     — true when denom > 0 and already canonical
 *
 * Arithmetic
 * ----------
 *   rc_rational_int_mul(a, b)   — a * b  (b integer)
 *   rc_rational_mul(a, b)       — a * b
 *   rc_rational_int_div(a, b)   — a / b  (b non-zero integer; sign-corrected)
 *   rc_rational_div(a, b)       — a / b  (b non-zero rational)
 *   rc_rational_int_add(a, b)   — a + b  (b integer)
 *   rc_rational_add(a, b)       — a + b  (result always in lowest terms)
 *   rc_rational_int_sub(a, b)   — a - b  (b integer)
 *   rc_rational_sub(a, b)       — a - b  (result always in lowest terms)
 *
 * All operations assert their pre-conditions (valid inputs, non-zero divisors).
 */

#ifndef RC_MATH_RATIONAL_H_
#define RC_MATH_RATIONAL_H_

#include "richc/math/math.h"
#include "richc/debug.h"

/* ---- type ---- */

typedef struct {
    int64_t num;
    int64_t denom;
} rc_rational;

/* ---- construction ---- */

/*
 * Reduce num/denom to canonical form.
 * Ensures denom > 0 (flips signs if needed) and divides both by their GCD.
 * Returns {0, 0} (invalid) if denom == 0.
 */
static inline rc_rational rc_rational_make(int64_t num, int64_t denom)
{
    if (denom == 0) return (rc_rational) {0, 0};
    if (denom < 0) { num = -num; denom = -denom; }
    int64_t d = rc_gcd_i64(num, denom);
    return (rc_rational) {num / d, denom / d};
}

/* Construct the integer n as a rational n/1. */
static inline rc_rational rc_rational_from_i64(int64_t num)
{
    return (rc_rational) {num, 1};
}

/* ---- predicates ---- */

/* True when the rational is in canonical form with a positive denominator. */
static inline bool rc_rational_is_valid(rc_rational a)
{
    return a.denom > 0 && rc_gcd_i64(a.num, a.denom) == 1;
}

/* ---- arithmetic ---- */

/* a * b  where b is an integer.  Result is reduced to lowest terms. */
static inline rc_rational rc_rational_int_mul(rc_rational a, int64_t b)
{
    RC_ASSERT(a.denom > 0);
    return rc_rational_make(a.num * b, a.denom);
}

/* a * b.  Result is reduced to lowest terms. */
static inline rc_rational rc_rational_mul(rc_rational a, rc_rational b)
{
    RC_ASSERT(a.denom > 0);
    RC_ASSERT(b.denom > 0);
    return rc_rational_make(a.num * b.num, a.denom * b.denom);
}

/*
 * a / b  where b is a non-zero integer.
 *
 * Passes (num, denom * b) directly to rc_rational_make, which handles sign
 * normalisation (flipping both signs when denom*b < 0) and GCD reduction.
 */
static inline rc_rational rc_rational_int_div(rc_rational a, int64_t b)
{
    RC_ASSERT(a.denom > 0);
    RC_ASSERT(b != 0);
    return rc_rational_make(a.num, a.denom * b);
}

/* a / b.  b.num must be non-zero. */
static inline rc_rational rc_rational_div(rc_rational a, rc_rational b)
{
    RC_ASSERT(a.denom > 0);
    RC_ASSERT(b.num != 0);
    RC_ASSERT(b.denom > 0);
    return rc_rational_make(a.num * b.denom, a.denom * b.num);
}

/* a + b  where b is an integer. */
static inline rc_rational rc_rational_int_add(rc_rational a, int64_t b)
{
    RC_ASSERT(a.denom > 0);
    return (rc_rational) {a.num + b * a.denom, a.denom};
}

/*
 * a + b.  Result is always reduced to lowest terms.
 *
 * Uses the GCD of the two denominators to minimise intermediate values:
 *   d  = gcd(a.denom, b.denom)
 *   da = a.denom / d,  db = b.denom / d
 *   num   = a.num * db + b.num * da
 *   denom = da * b.denom  (= lcm(a.denom, b.denom))
 * The result is then passed through rc_rational_make to ensure GCD == 1.
 */
static inline rc_rational rc_rational_add(rc_rational a, rc_rational b)
{
    RC_ASSERT(a.denom > 0);
    RC_ASSERT(b.denom > 0);
    int64_t d  = rc_gcd_i64(a.denom, b.denom);
    int64_t da = a.denom / d;
    int64_t db = b.denom / d;
    return rc_rational_make(a.num * db + b.num * da, da * b.denom);
}

/* a - b  where b is an integer. */
static inline rc_rational rc_rational_int_sub(rc_rational a, int64_t b)
{
    RC_ASSERT(a.denom > 0);
    return (rc_rational) {a.num - b * a.denom, a.denom};
}

/*
 * a - b.  Result is always reduced to lowest terms.
 * Same algorithm as rc_rational_add with subtraction in the numerator.
 */
static inline rc_rational rc_rational_sub(rc_rational a, rc_rational b)
{
    RC_ASSERT(a.denom > 0);
    RC_ASSERT(b.denom > 0);
    int64_t d  = rc_gcd_i64(a.denom, b.denom);
    int64_t da = a.denom / d;
    int64_t db = b.denom / d;
    return rc_rational_make(a.num * db - b.num * da, da * b.denom);
}

#endif /* RC_MATH_RATIONAL_H_ */
