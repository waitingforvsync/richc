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
 *   rc_rational_make(num, denom)            — canonical form; denom==0 → invalid
 *   rc_rational_from_i64(n)                 — integer n/1  (inline)
 *   rc_rational_from_double(val, threshold) — simplest rational within ±threshold of val
 *
 * Predicates  (all inline)
 * ------------------------
 *   rc_rational_is_valid(a)     — true when denom > 0 and already canonical
 *   rc_rational_is_zero(a)      — true when num == 0
 *   rc_rational_is_integer(a)   — true when denom == 1
 *   rc_rational_is_positive(a)  — true when num > 0
 *   rc_rational_is_negative(a)  — true when num < 0
 *
 * Arithmetic
 * ----------
 *   rc_rational_negate(a)       — -a                            (inline)
 *   rc_rational_abs(a)          — |a|                           (inline)
 *   rc_rational_reciprocal(a)   — 1/a  (asserts a != 0)         (inline)
 *   rc_rational_int_mul(a, b)   — a * b  (b integer)
 *   rc_rational_mul(a, b)       — a * b
 *   rc_rational_int_div(a, b)   — a / b  (b non-zero integer)
 *   rc_rational_div(a, b)       — a / b  (b non-zero rational)
 *   rc_rational_int_add(a, b)   — a + b  (b integer)            (inline)
 *   rc_rational_add(a, b)       — a + b  (result always in lowest terms)
 *   rc_rational_int_sub(a, b)   — a - b  (b integer)            (inline)
 *   rc_rational_sub(a, b)       — a - b  (result always in lowest terms)
 *
 * Comparison  (all inline)
 * ------------------------
 *   rc_rational_compare(a, b)        — -1 / 0 / +1
 *   rc_rational_is_equal(a, b)       — a == b
 *   rc_rational_is_less_than(a, b)   — a < b
 *   rc_rational_is_greater_than(a,b) — a > b
 *   rc_rational_min(a, b)            — lesser of a and b
 *   rc_rational_max(a, b)            — greater of a and b
 *
 * Conversion  (inline)
 * --------------------
 *   rc_rational_to_double(a)    — (double)num / (double)denom
 *
 * All operations assert their pre-conditions (valid inputs, non-zero divisors)
 * and assert that arithmetic results do not overflow int64_t.
 * Non-trivial operations are implemented in src/math/rational.c.
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

/* Reduce num/denom to canonical form; denom == 0 → invalid {0, 0}. */
rc_rational rc_rational_make(int64_t num, int64_t denom);

/* Construct the integer n as a rational n/1. */
static inline rc_rational rc_rational_from_i64(int64_t num)
{
    return (rc_rational) {num, 1};
}

/*
 * Find the simplest rational within threshold of val using the continued-
 * fraction algorithm.  See src/math/rational.c for a full description.
 * Asserts: val is finite, |val| < INT64_MAX, threshold >= 0.
 */
rc_rational rc_rational_from_double(double val, double threshold);

/* ---- predicates ---- */

/* True when the rational is in canonical form with a positive denominator. */
static inline bool rc_rational_is_valid(rc_rational a)
{
    return a.denom > 0 && rc_gcd_i64(a.num, a.denom) == 1;
}

/* True when the value is zero (num == 0; denom == 1 in canonical form). */
static inline bool rc_rational_is_zero(rc_rational a)
{
    RC_ASSERT(rc_rational_is_valid(a));
    return a.num == 0;
}

/* True when the value is an exact integer (denom == 1 in canonical form). */
static inline bool rc_rational_is_integer(rc_rational a)
{
    RC_ASSERT(rc_rational_is_valid(a));
    return a.denom == 1;
}

/* True when the value is strictly greater than zero. */
static inline bool rc_rational_is_positive(rc_rational a)
{
    RC_ASSERT(rc_rational_is_valid(a));
    return a.num > 0;
}

/* True when the value is strictly less than zero. */
static inline bool rc_rational_is_negative(rc_rational a)
{
    RC_ASSERT(rc_rational_is_valid(a));
    return a.num < 0;
}

/* ---- arithmetic ---- */

/* -a.  Asserts a.num != INT64_MIN. */
static inline rc_rational rc_rational_negate(rc_rational a)
{
    RC_ASSERT(rc_rational_is_valid(a));
    RC_ASSERT(a.num != INT64_MIN);
    return (rc_rational) {-a.num, a.denom};
}

/* |a|.  Asserts a.num != INT64_MIN. */
static inline rc_rational rc_rational_abs(rc_rational a)
{
    RC_ASSERT(rc_rational_is_valid(a));
    RC_ASSERT(a.num != INT64_MIN);
    return (rc_rational) {a.num < 0 ? -a.num : a.num, a.denom};
}

/* 1/a.  Asserts a != 0.  Result is canonical (sign moved to numerator if needed). */
static inline rc_rational rc_rational_reciprocal(rc_rational a)
{
    RC_ASSERT(rc_rational_is_valid(a));
    RC_ASSERT(a.num != 0);
    if (a.num < 0) return (rc_rational) {-a.denom, -a.num};
    return (rc_rational) {a.denom, a.num};
}

/* a * b  (b integer). */
rc_rational rc_rational_int_mul(rc_rational a, int64_t b);

/* a * b. */
rc_rational rc_rational_mul(rc_rational a, rc_rational b);

/* a / b  (b non-zero integer; sign normalised). */
rc_rational rc_rational_int_div(rc_rational a, int64_t b);

/* a / b  (b non-zero rational). */
rc_rational rc_rational_div(rc_rational a, rc_rational b);

/* a + b  (b integer). */
static inline rc_rational rc_rational_int_add(rc_rational a, int64_t b)
{
    RC_ASSERT(rc_rational_is_valid(a));
    RC_ASSERT(!rc_mul_overflows_i64(b, a.denom));
    int64_t bd = b * a.denom;
    RC_ASSERT(!rc_add_overflows_i64(a.num, bd));
    return (rc_rational) {a.num + bd, a.denom};
}

/* a + b.  Result is always in lowest terms. */
rc_rational rc_rational_add(rc_rational a, rc_rational b);

/* a - b  (b integer). */
static inline rc_rational rc_rational_int_sub(rc_rational a, int64_t b)
{
    RC_ASSERT(rc_rational_is_valid(a));
    RC_ASSERT(!rc_mul_overflows_i64(b, a.denom));
    int64_t bd = b * a.denom;
    RC_ASSERT(!rc_sub_overflows_i64(a.num, bd));
    return (rc_rational) {a.num - bd, a.denom};
}

/* a - b.  Result is always in lowest terms. */
rc_rational rc_rational_sub(rc_rational a, rc_rational b);

/* ---- comparison ---- */

/* Return -1, 0, or +1 according to the sign of (a - b). */
static inline int32_t rc_rational_compare(rc_rational a, rc_rational b)
{
    return (int32_t)rc_sgn_i64(rc_rational_sub(a, b).num);
}

/*
 * True when a == b.  Both being in canonical form means struct equality
 * suffices; no subtraction needed.
 */
static inline bool rc_rational_is_equal(rc_rational a, rc_rational b)
{
    RC_ASSERT(rc_rational_is_valid(a));
    RC_ASSERT(rc_rational_is_valid(b));
    return a.num == b.num && a.denom == b.denom;
}

/* True when a < b. */
static inline bool rc_rational_is_less_than(rc_rational a, rc_rational b)
{
    return rc_rational_sub(a, b).num < 0;
}

/* True when a > b. */
static inline bool rc_rational_is_greater_than(rc_rational a, rc_rational b)
{
    return rc_rational_sub(a, b).num > 0;
}

/* Return the lesser of a and b. */
static inline rc_rational rc_rational_min(rc_rational a, rc_rational b)
{
    return rc_rational_is_less_than(a, b) ? a : b;
}

/* Return the greater of a and b. */
static inline rc_rational rc_rational_max(rc_rational a, rc_rational b)
{
    return rc_rational_is_less_than(a, b) ? b : a;
}

/* ---- conversion ---- */

/* Convert to double.  Precision loss is possible for large num or denom. */
static inline double rc_rational_to_double(rc_rational a)
{
    RC_ASSERT(rc_rational_is_valid(a));
    return (double)a.num / (double)a.denom;
}

#endif /* RC_MATH_RATIONAL_H_ */
