/*
 * bitset.h - growable bit array with set-bit iteration.
 *
 * Bits are packed into an array of uint32_t words: bit i lives in word i/32 at
 * bit position i%32.
 *
 * Invariant: every bit at a position >= num is zero.  This lets get_next_set
 * scan whole words without a per-bit bounds check, and every mutator preserves
 * it (clearing the vacated bits on shrink, leaving freshly grown bits at 0).
 *
 * Type
 * ----
 *   rc_bitset  { uint32_t *data; uint32_t num; uint32_t cap; }
 *     num - number of addressable bits
 *     cap - capacity in bits; always a multiple of 32
 *
 *   Zero-initialise to obtain an empty, valid bitset:  rc_bitset bs = {0};
 *
 * Allocating operations (bitset.c)
 * --------------------------------
 *   rc_bitset_reserve(bs, min_bits, arena)
 *        Ensure capacity for at least min_bits, allocated exactly (rounded up to
 *        a whole word).  No-op when cap >= min_bits.  Asserts arena != NULL when
 *        it must reallocate, and on overflow.
 *   rc_bitset_resize(bs, new_num, arena)
 *        Set num to new_num.  Growing reserves exactly and leaves the new bits 0;
 *        shrinking zeroes the vacated bits to keep the invariant.  arena may be
 *        NULL when shrinking or when new_num <= cap.
 *   rc_bitset_push(bs, val, arena) -> uint32_t
 *        Append one bit set to val; return its index (the old num).  Grows
 *        geometrically (the larger of 2*cap, the request, or 64 bits).
 *   rc_bitset_push_n_zero(bs, n, arena) -> uint32_t
 *        Append n zero bits; return the first index.  Grows geometrically.
 *
 * Non-allocating operations (bitset.c)
 * ------------------------------------
 *   rc_bitset_reset(bs)                  - clear all bits; num and cap unchanged
 *   rc_bitset_get_next_set(bs, pos)      - index of the first set bit at >= pos,
 *                                          or RC_INDEX_NONE
 *
 * Inline operations
 * -----------------
 *   rc_bitset_set(bs, i)        - set bit i        (asserts i < num)
 *   rc_bitset_clear(bs, i)      - clear bit i      (asserts i < num)
 *   rc_bitset_is_set(bs, i)     - true if set      (asserts i < num)
 *   rc_bitset_get_first_set(bs) - first set bit index, or RC_INDEX_NONE
 *
 * Iteration idiom:
 *   for (uint32_t i = rc_bitset_get_first_set(&bs);
 *        i != RC_INDEX_NONE;
 *        i = rc_bitset_get_next_set(&bs, i + 1)) {
 *       // use i
 *   }
 */

#ifndef RC_BITSET_H_
#define RC_BITSET_H_

#include <stdbool.h>
#include <stdint.h>

#include "richc/macros.h"   // RC_ASSERT, RC_INDEX_NONE

typedef struct rc_arena rc_arena;   // only used by pointer here

/* ---- type ---- */

typedef struct rc_bitset {
    uint32_t *data;
    uint32_t  num;   /* number of addressable bits       */
    uint32_t  cap;   /* capacity in bits; multiple of 32 */
} rc_bitset;

/* ---- non-trivial operations (bitset.c) ---- */

void     rc_bitset_reserve(rc_bitset *bs, uint32_t min_bits, rc_arena *arena);
void     rc_bitset_resize(rc_bitset *bs, uint32_t new_num, rc_arena *arena);
uint32_t rc_bitset_push(rc_bitset *bs, bool val, rc_arena *arena);
uint32_t rc_bitset_push_n_zero(rc_bitset *bs, uint32_t n, rc_arena *arena);
void     rc_bitset_reset(rc_bitset *bs);
uint32_t rc_bitset_get_next_set(const rc_bitset *bs, uint32_t pos);

/* ---- inline operations ---- */

/* Set bit i.  Asserts i < bs->num. */
static inline void rc_bitset_set(rc_bitset *bs, uint32_t i)
{
    RC_ASSERT(i < bs->num);
    bs->data[i >> 5] |= 1u << (i & 31u);
}

/* Clear bit i.  Asserts i < bs->num. */
static inline void rc_bitset_clear(rc_bitset *bs, uint32_t i)
{
    RC_ASSERT(i < bs->num);
    bs->data[i >> 5] &= ~(1u << (i & 31u));
}

/* True if bit i is set.  Asserts i < bs->num. */
static inline bool rc_bitset_is_set(const rc_bitset *bs, uint32_t i)
{
    RC_ASSERT(i < bs->num);
    return (bs->data[i >> 5] >> (i & 31u)) & 1u;
}

/* Index of the first set bit, or RC_INDEX_NONE if none. */
static inline uint32_t rc_bitset_get_first_set(const rc_bitset *bs)
{
    return rc_bitset_get_next_set(bs, 0);
}

#endif /* RC_BITSET_H_ */
