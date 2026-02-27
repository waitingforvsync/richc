/*
 * math/math.h - scalar math utilities.
 *
 * Provides degree-to-radian conversion, integer min/max/sign, GCD,
 * count-leading-zeros, and signed/unsigned 64-bit overflow checks.
 *
 * Naming
 * ------
 * All functions carry an explicit type suffix to avoid collisions with
 * platform macros (Windows defines `min`/`max` as macros):
 *   _i32  — operates on int32_t
 *   _i64  — operates on int64_t
 *   _u32  — operates on uint32_t
 *   _u64  — operates on uint64_t
 */

#ifndef RC_MATH_MATH_H_
#define RC_MATH_MATH_H_

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>   /* abs, llabs */

/* ---- degree / radian conversion ---- */

/* Convert degrees to radians. */
static inline float rc_deg_to_rad(float degrees)
{
    return degrees * 0.0174532925f;
}

/* ---- integer min / max / sign ---- */

static inline int32_t rc_min_i32(int32_t a, int32_t b) { return (a < b) ? a : b; }
static inline int32_t rc_max_i32(int32_t a, int32_t b) { return (a > b) ? a : b; }
static inline int64_t rc_min_i64(int64_t a, int64_t b) { return (a < b) ? a : b; }
static inline int64_t rc_max_i64(int64_t a, int64_t b) { return (a > b) ? a : b; }

/* Return -1, 0, or +1 according to the sign of a. */
static inline int32_t rc_sgn_i32(int32_t a) { return (a < 0) ? -1 : (a > 0) ? 1 : 0; }
static inline int64_t rc_sgn_i64(int64_t a) { return (a < 0) ? -1 : (a > 0) ? 1 : 0; }

/* ---- greatest common divisor ---- */

/* Euclidean GCD; always returns a non-negative value. */
static inline int32_t rc_gcd_i32(int32_t a, int32_t b)
{
    while (b != 0) { int32_t t = b; b = a % b; a = t; }
    return abs(a);
}

static inline int64_t rc_gcd_i64(int64_t a, int64_t b)
{
    while (b != 0) { int64_t t = b; b = a % b; a = t; }
    return llabs(a);
}

/* ---- count leading zeros ---- */

/* Returns 32 for a == 0. */
static inline uint32_t rc_clz_u32(uint32_t a)
{
    if (a == 0) return 32;
#ifdef _MSC_VER
    return __lzcnt(a);
#else
    return (uint32_t)__builtin_clz(a);
#endif
}

/* Returns 64 for a == 0. */
static inline uint32_t rc_clz_u64(uint64_t a)
{
    if (a == 0) return 64;
#ifdef _MSC_VER
    return (uint32_t)__lzcnt64(a);
#else
    return (uint32_t)__builtin_clzll(a);
#endif
}

/* ---- unsigned 64-bit overflow checks ---- */

/* True if a * b would overflow uint64_t. */
static inline bool rc_mul_overflows_u64(uint64_t a, uint64_t b)
{
    return a != 0 && b > UINT64_MAX / a;
}

/* True if a + b would overflow uint64_t. */
static inline bool rc_add_overflows_u64(uint64_t a, uint64_t b)
{
    return b > UINT64_MAX - a;
}

/* ---- signed 64-bit overflow checks ---- */

/* True if a + b would overflow int64_t. */
static inline bool rc_add_overflows_i64(int64_t a, int64_t b)
{
    if (b > 0) return a > INT64_MAX - b;
    if (b < 0) return a < INT64_MIN - b;
    return false;
}

/* True if a - b would overflow int64_t. */
static inline bool rc_sub_overflows_i64(int64_t a, int64_t b)
{
    if (b < 0) return a > INT64_MAX + b;
    if (b > 0) return a < INT64_MIN + b;
    return false;
}

/* True if a * b would overflow int64_t. */
static inline bool rc_mul_overflows_i64(int64_t a, int64_t b)
{
    if (a == 0 || b == 0) return false;
    if (a > 0 && b > 0) return a > INT64_MAX / b;
    if (a < 0 && b < 0) return a < INT64_MAX / b;
    if (a > 0)          return b < INT64_MIN / a;  /* a > 0, b < 0 */
    return              a < INT64_MIN / b;          /* a < 0, b > 0 */
}

#endif /* RC_MATH_MATH_H_ */
