#include "richc/bitset.h"
#include "richc/math/math.h"

#include <string.h>

/*
 * ctz_: count trailing zeros.  x must be non-zero.
 *
 * Isolates the lowest set bit with x & (0u - x), then uses rc_clz_u32:
 *   ctz(x) = 31 - clz(x & -x)
 *
 * This relies on rc_clz_u32 from math.h and avoids a direct dependency on
 * compiler-specific intrinsics (__builtin_ctz / _BitScanForward).
 */
static uint32_t ctz_(uint32_t x)
{
    return 31u - rc_clz_u32(x & (0u - x));
}

/* ---- reserve ---- */

/*
 * Ensure capacity for at least min_bits bits.
 * Capacity is always a multiple of 32; grows by doubling from 32.
 * Newly allocated words are zeroed to maintain the invariant.
 */
void rc_bitset_reserve(rc_bitset *bs, uint32_t min_bits, rc_arena *a)
{
    if (min_bits <= bs->cap) return;

    RC_ASSERT(a != NULL && "bitset: arena required when reallocating");

    uint32_t new_cap = bs->cap > 32u ? bs->cap : 32u;
    while (new_cap < min_bits) {
        RC_ASSERT(new_cap <= UINT32_MAX / 2u && "bitset: capacity overflow");
        new_cap *= 2u;
    }
    /* new_cap is a multiple of 32 (starts at 32 and doubles). */

    uint32_t old_words = bs->cap >> 5;
    uint32_t new_words = new_cap >> 5;

    void *p = rc_arena_realloc(a, bs->data,
                               old_words * (uint32_t)sizeof(uint32_t),
                               new_words * (uint32_t)sizeof(uint32_t));
    RC_ASSERT(p != NULL && "bitset: arena OOM");

    bs->data = (uint32_t *)p;
    /* Zero new words to maintain invariant: bits >= num are always 0. */
    memset(bs->data + old_words, 0,
           (new_words - old_words) * sizeof(uint32_t));
    bs->cap = new_cap;
}

/* ---- resize ---- */

/*
 * Set num to new_num.  When growing, new bits are 0 (invariant ensures bits
 * beyond the old num were already 0; reserve zeroes any freshly allocated
 * words).  When shrinking, bits from new_num..num-1 are explicitly zeroed to
 * restore the invariant.
 */
void rc_bitset_resize(rc_bitset *bs, uint32_t new_num, rc_arena *a)
{
    if (new_num > bs->cap)
        rc_bitset_reserve(bs, new_num, a);

    if (new_num < bs->num) {
        /* Zero bits from new_num to bs->num-1. */
        uint32_t bit      = new_num & 31u;
        uint32_t new_words = (new_num + 31u) >> 5;
        uint32_t old_words = (bs->num + 31u) >> 5;

        /* Mask the partial last word (skip when new_num is word-aligned). */
        if (bit != 0u)
            bs->data[new_num >> 5] &= (1u << bit) - 1u;

        /* Zero any full words that fall entirely outside new_num. */
        if (old_words > new_words)
            memset(bs->data + new_words, 0,
                   (old_words - new_words) * sizeof(uint32_t));
    }
    /* Growing: bits old_num..new_num-1 are already 0 (invariant). */

    bs->num = new_num;
}

/* ---- reset ---- */

/* Clear all bits.  num and cap are unchanged. */
void rc_bitset_reset(rc_bitset *bs)
{
    uint32_t words = (bs->num + 31u) >> 5;
    memset(bs->data, 0, words * sizeof(uint32_t));
}

/* ---- get_next_set ---- */

/*
 * Return the index of the first set bit at position >= pos, or RC_INDEX_NONE.
 *
 * Strategy: examine the first (possibly partial) word by shifting right to
 * discard bits below pos, then scan remaining whole words.  The invariant
 * (bits >= num are 0) means any set bit found is a valid index — no per-bit
 * bounds check is needed inside the loop.
 */
uint32_t rc_bitset_get_next_set(const rc_bitset *bs, uint32_t pos)
{
    if (pos >= bs->num) return RC_INDEX_NONE;

    uint32_t words    = (bs->num + 31u) >> 5;
    uint32_t word_idx = pos >> 5;
    uint32_t bit_off  = pos & 31u;

    /* First word: mask off bits below pos by right-shifting. */
    uint32_t w = bs->data[word_idx] >> bit_off;
    if (w)
        return (word_idx << 5) + bit_off + ctz_(w);

    /* Scan remaining whole words. */
    for (word_idx++; word_idx < words; word_idx++) {
        w = bs->data[word_idx];
        if (w)
            return (word_idx << 5) + ctz_(w);
    }

    return RC_INDEX_NONE;
}
