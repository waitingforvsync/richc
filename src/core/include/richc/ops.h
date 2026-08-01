/*
 * ops.h - scalar bit and math operations.
 *
 * Small static inline helpers: bit reinterpretation, integer min/max/sign,
 * GCD, count-leading/trailing-zeros, population count, overflow checks, and
 * degree/radian conversion.
 *
 * Functions carry a scalar type suffix (i32/i64/u32/u64/f32/f64), which also
 * avoids collisions with platform macros (Windows defines min/max as macros).
 */

#ifndef RC_OPS_H_
#define RC_OPS_H_

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>   // abs, llabs

#ifdef _MSC_VER
#include <intrin.h>   // _BitScanReverse, _BitScanReverse64, _BitScanForward, _BitScanForward64
#endif

/* ---- bit reinterpretation ---- */

// Reinterpret a float as its uint32_t bit pattern.
static inline uint32_t rc_bitcast_f32(float x)
{
    union { float f; uint32_t u; } pun = {.f = x};
    return pun.u;
}

// Reinterpret a double as its uint64_t bit pattern.
static inline uint64_t rc_bitcast_f64(double x)
{
    union { double f; uint64_t u; } pun = {.f = x};
    return pun.u;
}

/* ---- min / max / sign ---- */

static inline int32_t rc_min_i32(int32_t a, int32_t b) { return (a < b) ? a : b; }
static inline int32_t rc_max_i32(int32_t a, int32_t b) { return (a > b) ? a : b; }
static inline int64_t rc_min_i64(int64_t a, int64_t b) { return (a < b) ? a : b; }
static inline int64_t rc_max_i64(int64_t a, int64_t b) { return (a > b) ? a : b; }
static inline float   rc_min_f32(float a, float b)     { return (a < b) ? a : b; }
static inline float   rc_max_f32(float a, float b)     { return (a > b) ? a : b; }
static inline double  rc_min_f64(double a, double b)   { return (a < b) ? a : b; }
static inline double  rc_max_f64(double a, double b)   { return (a > b) ? a : b; }

// Return -1, 0, or +1 according to the sign of a.
static inline int32_t rc_sgn_i32(int32_t a) { return (a < 0) ? -1 : (a > 0) ? 1 : 0; }
static inline int64_t rc_sgn_i64(int64_t a) { return (a < 0) ? -1 : (a > 0) ? 1 : 0; }

/* ---- greatest common divisor (Euclidean; always non-negative) ---- */

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

/* ---- count leading zeros (returns the bit width for 0) ---- */

static inline uint32_t rc_clz_u32(uint32_t a)
{
    if (a == 0) return 32;
#ifdef _MSC_VER
    unsigned long index = 0;
    _BitScanReverse(&index, a);   // a != 0 here, so index is always written
    return 31u - index;
#else
    return (uint32_t)__builtin_clz(a);
#endif
}

static inline uint32_t rc_clz_u64(uint64_t a)
{
    if (a == 0) return 64;
#ifdef _MSC_VER
    unsigned long index = 0;
    _BitScanReverse64(&index, a);   // a != 0 here, so index is always written
    return 63u - index;
#else
    return (uint32_t)__builtin_clzll(a);
#endif
}

/* ---- count trailing zeros (returns the bit width for 0) ---- */

static inline uint32_t rc_ctz_u32(uint32_t a)
{
    if (a == 0) return 32;
#ifdef _MSC_VER
    unsigned long index = 0;
    _BitScanForward(&index, a);   // a != 0 here, so index is always written
    return index;
#else
    return (uint32_t)__builtin_ctz(a);
#endif
}

static inline uint32_t rc_ctz_u64(uint64_t a)
{
    if (a == 0) return 64;
#ifdef _MSC_VER
    unsigned long index = 0;
    _BitScanForward64(&index, a);   // a != 0 here, so index is always written
    return index;
#else
    return (uint32_t)__builtin_ctzll(a);
#endif
}

/* ---- population count (number of set bits) ---- */

static inline uint32_t rc_popcount_u32(uint32_t a)
{
#ifdef _MSC_VER
    return (uint32_t)__popcnt(a);
#else
    return (uint32_t)__builtin_popcount(a);
#endif
}

/* ---- overflow checks ---- */

// True if a * b would overflow uint64_t.
static inline bool rc_mul_overflows_u64(uint64_t a, uint64_t b)
{
    return a != 0 && b > UINT64_MAX / a;
}

// True if a + b would overflow uint64_t.
static inline bool rc_add_overflows_u64(uint64_t a, uint64_t b)
{
    return b > UINT64_MAX - a;
}

// True if a + b would overflow int64_t.
static inline bool rc_add_overflows_i64(int64_t a, int64_t b)
{
    if (b > 0) return a > INT64_MAX - b;
    if (b < 0) return a < INT64_MIN - b;
    return false;
}

// True if a - b would overflow int64_t.
static inline bool rc_sub_overflows_i64(int64_t a, int64_t b)
{
    if (b < 0) return a > INT64_MAX + b;
    if (b > 0) return a < INT64_MIN + b;
    return false;
}

// True if a * b would overflow int64_t.
static inline bool rc_mul_overflows_i64(int64_t a, int64_t b)
{
    if (a == 0 || b == 0) return false;
    if (a > 0 && b > 0) return a > INT64_MAX / b;
    if (a < 0 && b < 0) return a < INT64_MAX / b;
    if (a > 0)          return b < INT64_MIN / a;   // a > 0, b < 0
    return              a < INT64_MIN / b;          // a < 0, b > 0
}

/* ---- degree / radian conversion ---- */

// Convert degrees to radians.
static inline float rc_deg_to_rad(float degrees)
{
    return degrees * 0.0174532925f;
}

#endif /* RC_OPS_H_ */
