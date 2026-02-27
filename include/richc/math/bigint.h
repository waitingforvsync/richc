/*
 * math/bigint.h - arbitrary-precision integer (rc_bigint).
 *
 * Representation
 * --------------
 * Sign-magnitude: a magnitude stored as an array of uint32_t limbs in
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
 * capacity is 8 limbs (= 32 bytes); growing doubles capacity, subject to the
 * 8-limb floor.
 *
 * Aliasing
 * --------
 * All binary operations use the 3-address form: result, b, c are three
 * separate parameters, any of which may alias each other (result==b, result==c,
 * b==c, or all three equal).  All input data is captured before any
 * reallocation, so no branch logic is needed at call sites.
 *
 *   rc_bigint_add(result, b, c, arena)      — result = b + c
 *   rc_bigint_sub(result, b, c, arena)      — result = b - c
 *   rc_bigint_mul(result, b, c, arena, scratch) — result = b * c
 *
 * For divmod: quot and rem must not alias each other, but each may alias b
 * or c independently.
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
 *   rc_bigint_add(result, b, c, arena)       — result = b + c
 *   rc_bigint_sub(result, b, c, arena)       — result = b - c
 *   rc_bigint_mul(result, b, c, arena, scratch) — result = b * c
 *   rc_bigint_divmod(quot, rem, b, c, arena, scratch) — quot = b/c, rem = b%c
 *   rc_bigint_div(result, b, c, arena, scratch)       — result = b / c
 *   rc_bigint_mod(result, b, c, arena, scratch)       — result = b % c
 *
 * Division sign convention (truncating toward zero):
 *   quot_sign = sign(b) * sign(c),  rem_sign = sign(b).
 *   Asserts c != 0.
 *
 * Integer shortcuts (inline; c is int64_t)
 * ----------------------------------------
 *   rc_bigint_int_add/sub/mul/divmod/div/mod — same as above but c is int64_t.
 *   Constructs a transient stack-backed rc_bigint and delegates to the
 *   corresponding 3-address function.  No arena allocation for c.
 *
 * Conversion (inline)
 * -------------------
 *   rc_bigint_to_u64(a)  — asserts value fits in uint64_t (at most 2 limbs)
 *   rc_bigint_to_i64(a)  — asserts value fits in int64_t  (at most 2 limbs)
 */

#ifndef RC_MATH_BIGINT_H_
#define RC_MATH_BIGINT_H_

#include <stdint.h>
#include <stdbool.h>
#include "richc/arena.h"
#include "richc/debug.h"

/* ---- type ---- */

typedef struct {
    uint32_t *digits;   /* limbs in little-endian order                    */
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

/* result = b + c.  result may alias b and/or c. */
void rc_bigint_add(rc_bigint *result, const rc_bigint *b, const rc_bigint *c,
                   rc_arena *arena);

/* result = b - c.  result may alias b and/or c. */
void rc_bigint_sub(rc_bigint *result, const rc_bigint *b, const rc_bigint *c,
                   rc_arena *arena);

/*
 * result = b * c.  result may alias b and/or c.
 * The product is computed into a temporary buffer in scratch (passed by value,
 * discarded on return), then copied back into result via the main arena.
 */
void rc_bigint_mul(rc_bigint *result, const rc_bigint *b, const rc_bigint *c,
                   rc_arena *arena, rc_arena scratch);

/*
 * quot = b / c, rem = b % c  (truncating toward zero).
 * rem has the sign of b; quot has sign(b)*sign(c).
 * quot and rem must not alias each other, but may each alias b or c.
 * Asserts c != 0.
 */
void rc_bigint_divmod(rc_bigint *quot, rc_bigint *rem,
                      const rc_bigint *b, const rc_bigint *c,
                      rc_arena *arena, rc_arena scratch);

/* result = b / c  (truncating toward zero).  Asserts c != 0. */
void rc_bigint_div(rc_bigint *result, const rc_bigint *b, const rc_bigint *c,
                   rc_arena *arena, rc_arena scratch);

/* result = b % c  (remainder has sign of b).  Asserts c != 0. */
void rc_bigint_mod(rc_bigint *result, const rc_bigint *b, const rc_bigint *c,
                   rc_arena *arena, rc_arena scratch);

/* ---- integer shortcut operations (inline) ---- */

/*
 * Each rc_bigint_int_* function accepts an int64_t in place of the second
 * const rc_bigint *.  It constructs a transient rc_bigint on the stack backed
 * by a 2-limb array, then delegates to the corresponding 3-address function.
 * result may alias b.
 *
 *   rc_bigint_int_add(result, b, c, arena)             — result = b + c
 *   rc_bigint_int_sub(result, b, c, arena)             — result = b - c
 *   rc_bigint_int_mul(result, b, c, arena, scratch)    — result = b * c
 *   rc_bigint_int_divmod(quot, rem, b, c, arena, scratch) — divmod
 *   rc_bigint_int_div(result, b, c, arena, scratch)    — result = b / c
 *   rc_bigint_int_mod(result, b, c, arena, scratch)    — result = b % c
 */

/*
 * Private helper: fills limbs[2] from v and returns a temporary rc_bigint
 * backed by it.  Valid only while limbs[] remains in scope.
 */
static inline rc_bigint rc_bigint_make_i64_tmp_(int64_t v, uint32_t limbs[2])
{
    if (v == 0) return (rc_bigint) {limbs, 0, 2, 0};
    uint64_t mag = v < 0 ? (uint64_t)0 - (uint64_t)v : (uint64_t)v;
    int32_t  sgn = v < 0 ? -1 : 1;
    limbs[0] = (uint32_t)mag;
    if (mag > (uint64_t)UINT32_MAX) {
        limbs[1] = (uint32_t)(mag >> 32);
        return (rc_bigint) {limbs, 2, 2, sgn};
    }
    return (rc_bigint) {limbs, 1, 2, sgn};
}

static inline void rc_bigint_int_add(rc_bigint *result, const rc_bigint *b,
                                      int64_t c, rc_arena *arena)
{
    uint32_t limbs[2];
    rc_bigint tmp = rc_bigint_make_i64_tmp_(c, limbs);
    rc_bigint_add(result, b, &tmp, arena);
}

static inline void rc_bigint_int_sub(rc_bigint *result, const rc_bigint *b,
                                      int64_t c, rc_arena *arena)
{
    uint32_t limbs[2];
    rc_bigint tmp = rc_bigint_make_i64_tmp_(c, limbs);
    rc_bigint_sub(result, b, &tmp, arena);
}

static inline void rc_bigint_int_mul(rc_bigint *result, const rc_bigint *b,
                                      int64_t c, rc_arena *arena, rc_arena scratch)
{
    uint32_t limbs[2];
    rc_bigint tmp = rc_bigint_make_i64_tmp_(c, limbs);
    rc_bigint_mul(result, b, &tmp, arena, scratch);
}

static inline void rc_bigint_int_divmod(rc_bigint *quot, rc_bigint *rem,
                                         const rc_bigint *b, int64_t c,
                                         rc_arena *arena, rc_arena scratch)
{
    uint32_t limbs[2];
    rc_bigint tmp = rc_bigint_make_i64_tmp_(c, limbs);
    rc_bigint_divmod(quot, rem, b, &tmp, arena, scratch);
}

static inline void rc_bigint_int_div(rc_bigint *result, const rc_bigint *b,
                                      int64_t c, rc_arena *arena, rc_arena scratch)
{
    uint32_t limbs[2];
    rc_bigint tmp = rc_bigint_make_i64_tmp_(c, limbs);
    rc_bigint_div(result, b, &tmp, arena, scratch);
}

static inline void rc_bigint_int_mod(rc_bigint *result, const rc_bigint *b,
                                      int64_t c, rc_arena *arena, rc_arena scratch)
{
    uint32_t limbs[2];
    rc_bigint tmp = rc_bigint_make_i64_tmp_(c, limbs);
    rc_bigint_mod(result, b, &tmp, arena, scratch);
}

/* ---- conversion (inline) ---- */

/*
 * Convert to uint64_t.  Asserts the value is non-negative and fits in
 * at most two limbs (i.e. 0 <= value <= UINT64_MAX).
 */
static inline uint64_t rc_bigint_to_u64(const rc_bigint *a)
{
    RC_ASSERT(rc_bigint_is_valid(a));
    RC_ASSERT(a->sign >= 0);
    if (a->sign == 0) return 0;
    RC_ASSERT(a->len <= 2);
    uint64_t lo = a->digits[0];
    uint64_t hi = a->len >= 2 ? (uint64_t)a->digits[1] << 32 : 0;
    return lo | hi;
}

/*
 * Convert to int64_t.  Asserts the value fits in int64_t (at most two limbs).
 * The INT64_MIN case (magnitude == 2^63) is handled correctly.
 */
static inline int64_t rc_bigint_to_i64(const rc_bigint *a)
{
    RC_ASSERT(rc_bigint_is_valid(a));
    if (a->sign == 0) return 0;
    RC_ASSERT(a->len <= 2);
    uint64_t mag = (uint64_t)a->digits[0]
                 | (a->len >= 2 ? (uint64_t)a->digits[1] << 32 : 0);
    if (a->sign > 0) {
        RC_ASSERT(mag <= (uint64_t)INT64_MAX);
        return (int64_t)mag;
    } else {
        RC_ASSERT(mag <= (uint64_t)INT64_MAX + 1);
        return mag == (uint64_t)INT64_MAX + 1
            ? INT64_MIN
            : -(int64_t)mag;
    }
}

#endif /* RC_MATH_BIGINT_H_ */
