/*
 * bitset.h - fixed-width bit array with set-bit iteration.
 *
 * Stores bits packed into an array of uint32_t words.  The bit at index i
 * lives in word i/32 at bit position i%32.
 *
 * Invariant: all bits at positions >= num are always zero.  This lets
 * get_next_set scan words without a per-bit bounds check.
 *
 * Type
 * ----
 *   rc_bitset  { uint32_t *data; uint32_t num; uint32_t cap; }
 *
 *   num — number of addressable bits
 *   cap — allocated capacity in bits; always a multiple of 32
 *
 *   Zero-initialise to obtain an empty, valid bitset:
 *     rc_bitset bs = {0};
 *
 * Allocating operations (bitset.c)
 * ---------------------------------
 *   rc_bitset_reserve(bs, min_bits, a)
 *        Ensure capacity for at least min_bits.  No-op if cap >= min_bits.
 *        Grows by doubling from 32.  Asserts a != NULL when reallocation
 *        is needed, and on overflow or OOM.
 *
 *   rc_bitset_resize(bs, new_num, a)
 *        Set num to new_num, growing capacity via reserve if needed.
 *        New bits (when growing) are zero.  Shrinking zeroes the vacated
 *        bits to maintain the invariant.  Arena may be NULL when shrinking
 *        or when new_num <= cap.
 *
 * Non-allocating operations (bitset.c)
 * --------------------------------------
 *   rc_bitset_reset(bs)
 *        Clear all bits without changing num or cap.
 *
 *   rc_bitset_get_next_set(bs, pos)
 *        Return the index of the first set bit at position >= pos, or
 *        RC_INDEX_NONE if no such bit exists.
 *
 * Inline operations
 * -----------------
 *   rc_bitset_set(bs, i)       — set bit i          (asserts i < num)
 *   rc_bitset_clear(bs, i)     — clear bit i        (asserts i < num)
 *   rc_bitset_is_set(bs, i)    — 1 if bit i set     (asserts i < num)
 *   rc_bitset_get_first_set(bs) — first set bit index, or RC_INDEX_NONE
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

#include <stdint.h>
#include "richc/arena.h"
#include "richc/debug.h"
#include "richc/template_util.h"

/* ---- type ---- */

typedef struct {
    uint32_t *data;
    uint32_t  num;   /* number of addressable bits          */
    uint32_t  cap;   /* capacity in bits; multiple of 32    */
} rc_bitset;

/* ---- non-trivial operations (bitset.c) ---- */

void     rc_bitset_reserve     (rc_bitset *bs, uint32_t min_bits, rc_arena *a);
void     rc_bitset_resize      (rc_bitset *bs, uint32_t new_num,  rc_arena *a);
void     rc_bitset_reset       (rc_bitset *bs);
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

/* Return 1 if bit i is set, 0 otherwise.  Asserts i < bs->num. */
static inline int rc_bitset_is_set(const rc_bitset *bs, uint32_t i)
{
    RC_ASSERT(i < bs->num);
    return (int)((bs->data[i >> 5] >> (i & 31u)) & 1u);
}

/* Return the index of the first set bit, or RC_INDEX_NONE if none. */
static inline uint32_t rc_bitset_get_first_set(const rc_bitset *bs)
{
    return rc_bitset_get_next_set(bs, 0);
}

#endif /* RC_BITSET_H_ */
