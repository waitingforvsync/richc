/*
 * math/bigint.h - arbitrary-precision integer (rc_bigint).
 *
 * Representation
 * --------------
 * Sign-magnitude: a magnitude stored as an array of uint64_t limbs in
 * little-endian order (index 0 = least significant limb), plus a separate
 * sign field.  The magnitude is always normalised: no leading zero limbs, so
 * len == 0 if and only if the value is zero.
 *
 * States
 * ------
 *   Invalid : digits == NULL  — not initialised.
 *   Valid   : digits non-NULL, len <= cap, sign in {-1, 0, +1},
 *             digits[len-1] != 0 (no leading zeros; vacuously true when len==0)
 *
 * Memory
 * ------
 * The digits buffer is arena-backed (rc_arena).  Functions that may grow the
 * buffer accept an rc_arena * as their last parameter.  The default initial
 * capacity is 8 limbs (= 64 bytes); growing doubles capacity, subject to the
 * 8-limb floor.
 *
 * Aliasing
 * --------
 * rc_bigint_add(a, b, arena)  handles a == b (self-add).
 * rc_bigint_add3(result, b, c, arena) handles result == b and result == c.
 * Having two *different* rc_bigint structs share the same digits buffer is
 * not supported and will produce incorrect results.
 *
 * Construction (return by value)
 * --------------------------------
 *   rc_bigint_make(cap, arena)       — zero value, initial capacity >= max(cap,8)
 *   rc_bigint_from_u64(v, arena)     — construct from uint64_t
 *   rc_bigint_from_i64(v, arena)     — construct from int64_t (including INT64_MIN)
 *   rc_bigint_copy(src, arena)       — copy into a new arena buffer
 *
 * Predicates (all inline)
 * -----------------------
 *   rc_bigint_is_valid(a)            — true when digits != NULL
 *   rc_bigint_is_zero(a)             — true when sign == 0
 *   rc_bigint_is_positive(a)         — true when sign > 0
 *   rc_bigint_is_negative(a)         — true when sign < 0
 *
 * Mutation
 * --------
 *   rc_bigint_reset(a)               — set to zero, retain buffer  (inline)
 *   rc_bigint_negate(a)              — negate in-place             (inline)
 *   rc_bigint_reserve(a, cap, arena) — ensure at least cap limbs
 *   rc_bigint_add(a, b, arena)       — a += b
 *   rc_bigint_sub(a, b, arena)       — a -= b
 *   rc_bigint_add3(r, b, c, arena)   — r = b + c  (r must be pre-initialised)
 *   rc_bigint_sub3(r, b, c, arena)   — r = b - c  (r must be pre-initialised)
 *
 * Conversion (inline)
 * -------------------
 *   rc_bigint_to_u64(a)  — asserts value fits in uint64_t
 *   rc_bigint_to_i64(a)  — asserts value fits in int64_t
 */

#ifndef RC_MATH_BIGINT_H_
#define RC_MATH_BIGINT_H_

#include <stdint.h>
#include <stdbool.h>
#include "richc/arena.h"
#include "richc/debug.h"

/* ---- type ---- */

typedef struct {
    uint64_t *digits;   /* limbs in little-endian order                    */
    uint32_t  len;      /* number of significant limbs; 0 when value is 0  */
    uint32_t  cap;      /* total allocated limbs                           */
    int32_t   sign;     /* -1, 0, or +1                                    */
} rc_bigint;

/* ---- construction ---- */

/*
 * Allocate a zero-valued bigint with initial capacity >= max(cap, 8).
 * cap == 0 is valid and uses the default capacity of 8.
 */
rc_bigint rc_bigint_make(uint32_t cap, rc_arena *a);

/* Construct from an unsigned 64-bit integer. */
rc_bigint rc_bigint_from_u64(uint64_t v, rc_arena *a);

/* Construct from a signed 64-bit integer.  INT64_MIN is handled correctly. */
rc_bigint rc_bigint_from_i64(int64_t v, rc_arena *a);

/* Construct a copy of src in a new arena buffer. */
rc_bigint rc_bigint_copy(const rc_bigint *src, rc_arena *a);

/* ---- predicates (inline) ---- */

/* True when the bigint holds a valid (non-NULL) digits buffer. */
static inline bool rc_bigint_is_valid(const rc_bigint *a)
{
    return a->digits != NULL;
}

/* True when the value is zero.  Asserts valid. */
static inline bool rc_bigint_is_zero(const rc_bigint *a)
{
    RC_ASSERT(rc_bigint_is_valid(a));
    return a->sign == 0;
}

/* True when the value is strictly greater than zero.  Asserts valid. */
static inline bool rc_bigint_is_positive(const rc_bigint *a)
{
    RC_ASSERT(rc_bigint_is_valid(a));
    return a->sign > 0;
}

/* True when the value is strictly less than zero.  Asserts valid. */
static inline bool rc_bigint_is_negative(const rc_bigint *a)
{
    RC_ASSERT(rc_bigint_is_valid(a));
    return a->sign < 0;
}

/* ---- mutation (inline) ---- */

/* Set to zero, retain buffer. */
static inline void rc_bigint_reset(rc_bigint *a)
{
    RC_ASSERT(rc_bigint_is_valid(a));
    a->len  = 0;
    a->sign = 0;
}

/* Negate in-place.  No-op on zero. */
static inline void rc_bigint_negate(rc_bigint *a)
{
    RC_ASSERT(rc_bigint_is_valid(a));
    a->sign = -a->sign;
}

/* ---- mutation (non-trivial) ---- */

/* Ensure at least new_cap limbs of capacity, growing by doubling if needed. */
void rc_bigint_reserve(rc_bigint *a, uint32_t new_cap, rc_arena *arena);

/* a += b.  Handles a == b (self-add). */
void rc_bigint_add(rc_bigint *a, const rc_bigint *b, rc_arena *arena);

/* a -= b.  Handles a == b (self-sub, result is zero). */
void rc_bigint_sub(rc_bigint *a, const rc_bigint *b, rc_arena *arena);

/*
 * result = b + c.  result must already be initialised.
 * Handles result == b and result == c.
 */
void rc_bigint_add3(rc_bigint *result, const rc_bigint *b, const rc_bigint *c,
                    rc_arena *arena);

/*
 * result = b - c.  result must already be initialised.
 * Handles result == b and result == c.
 */
void rc_bigint_sub3(rc_bigint *result, const rc_bigint *b, const rc_bigint *c,
                    rc_arena *arena);

/* ---- conversion (inline) ---- */

/*
 * Convert to uint64_t.  Asserts the value is non-negative and fits in one
 * limb (i.e. 0 <= value <= UINT64_MAX).
 */
static inline uint64_t rc_bigint_to_u64(const rc_bigint *a)
{
    RC_ASSERT(rc_bigint_is_valid(a));
    RC_ASSERT(a->sign >= 0);
    if (a->sign == 0) return 0;
    RC_ASSERT(a->len == 1);
    return a->digits[0];
}

/*
 * Convert to int64_t.  Asserts the value fits in int64_t.
 * The INT64_MIN case (single limb with value 2^63) is handled correctly.
 */
static inline int64_t rc_bigint_to_i64(const rc_bigint *a)
{
    RC_ASSERT(rc_bigint_is_valid(a));
    if (a->sign == 0) return 0;
    RC_ASSERT(a->len == 1);
    if (a->sign > 0) {
        RC_ASSERT(a->digits[0] <= (uint64_t)INT64_MAX);
        return (int64_t)a->digits[0];
    } else {
        RC_ASSERT(a->digits[0] <= (uint64_t)INT64_MAX + 1);
        return a->digits[0] == (uint64_t)INT64_MAX + 1
            ? INT64_MIN
            : -(int64_t)a->digits[0];
    }
}

#endif /* RC_MATH_BIGINT_H_ */
