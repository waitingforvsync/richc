#include "richc/bitset.h"

#include <string.h>

#include "richc/arena.h"
#include "richc/ops.h"

/* Number of 32-bit words needed to hold `bits` bits. */
static uint32_t words_for(uint32_t bits)
{
    return (bits + 31u) >> 5;
}

/* ---- reserve ---- */

/*
 * Ensure capacity for at least min_bits, allocated exactly (rounded up to a
 * whole word).  Freshly allocated words are zeroed so the invariant (bits >= num
 * are 0) holds for any future set bits.
 */
void rc_bitset_reserve(rc_bitset *bs, uint32_t min_bits, rc_arena *arena)
{
    if (min_bits <= bs->cap) {
        return;
    }
    RC_ASSERT(arena != NULL);

    uint32_t new_cap = (min_bits + 31u) & ~31u;   // round up to a whole word
    RC_ASSERT(new_cap >= min_bits);               // the round-up did not overflow

    uint32_t old_words = bs->cap >> 5;
    uint32_t new_words = new_cap >> 5;
    RC_ASSERT(new_words <= UINT32_MAX / (uint32_t)sizeof(uint32_t));

    // realloc_zero zeroes the grown words, so any future set bit there reads 0
    bs->data = rc_arena_realloc_zero(arena, bs->data,
                                     old_words * (uint32_t)sizeof(uint32_t),
                                     new_words * (uint32_t)sizeof(uint32_t));
    bs->cap = new_cap;
}

/* ---- grow (geometric) ---- */

/*
 * Grow capacity to fit at least min_bits using the array growth policy: the
 * larger of double the current capacity, the request, or 64 bits (8 bytes).
 */
static void grow(rc_bitset *bs, uint32_t min_bits, rc_arena *arena)
{
    if (min_bits <= bs->cap) {
        return;
    }
    uint32_t doubled = bs->cap * 2u;   // wraps on overflow; the request dominates below
    uint32_t new_cap = min_bits > doubled ? min_bits : doubled;
    if (new_cap < 64u) {
        new_cap = 64u;
    }
    rc_bitset_reserve(bs, new_cap, arena);
}

/* ---- resize ---- */

/*
 * Set num to new_num.  Growing reserves exactly; the new bits are already 0
 * (the invariant kept bits beyond the old num zero, and reserve zeroes any new
 * words).  Shrinking zeroes the vacated bits [new_num, num) to keep the
 * invariant.
 */
void rc_bitset_resize(rc_bitset *bs, uint32_t new_num, rc_arena *arena)
{
    if (new_num > bs->num) {
        rc_bitset_reserve(bs, new_num, arena);
    }
    else if (new_num < bs->num) {
        // mask the partial word that straddles new_num (skip when word-aligned)
        uint32_t bit = new_num & 31u;
        if (bit != 0u) {
            bs->data[new_num >> 5] &= (1u << bit) - 1u;
        }
        // zero any full words that fall entirely above new_num
        uint32_t new_words = words_for(new_num);
        uint32_t old_words = words_for(bs->num);
        if (old_words > new_words) {
            memset(bs->data + new_words, 0, (old_words - new_words) * sizeof(uint32_t));
        }
    }
    bs->num = new_num;
}

/* ---- push ---- */

/* Append one bit set to val; return its index. */
uint32_t rc_bitset_push(rc_bitset *bs, bool val, rc_arena *arena)
{
    uint32_t i = bs->num;
    grow(bs, i + 1u, arena);
    bs->num = i + 1u;
    if (val) {
        bs->data[i >> 5] |= 1u << (i & 31u);
    }
    // val == false: bit i is already 0 by the invariant
    return i;
}

/* Append n zero bits; return the first index. */
uint32_t rc_bitset_push_n_zero(rc_bitset *bs, uint32_t n, rc_arena *arena)
{
    uint32_t start = bs->num;
    grow(bs, start + n, arena);
    bs->num = start + n;
    // the appended bits are already 0 (invariant + reserve zeroes new words)
    return start;
}

/* ---- reset ---- */

/* Clear all bits.  num and cap are unchanged. */
void rc_bitset_reset(rc_bitset *bs)
{
    uint32_t words = words_for(bs->num);
    if (words != 0) {
        memset(bs->data, 0, words * sizeof(uint32_t));
    }
}

/* ---- get_next_set ---- */

/*
 * Return the index of the first set bit at position >= pos, or RC_INDEX_NONE.
 *
 * Examine the first (possibly partial) word by shifting away bits below pos,
 * then scan the remaining whole words.  Because bits >= num are 0 (invariant),
 * any set bit found is a valid index - no per-bit bounds check is needed.
 */
uint32_t rc_bitset_get_next_set(const rc_bitset *bs, uint32_t pos)
{
    if (pos >= bs->num) {
        return RC_INDEX_NONE;
    }

    uint32_t words    = words_for(bs->num);
    uint32_t word_idx = pos >> 5;
    uint32_t bit_off  = pos & 31u;

    // first word: drop bits below pos
    uint32_t w = bs->data[word_idx] >> bit_off;
    if (w != 0) {
        return (word_idx << 5) + bit_off + rc_ctz_u32(w);
    }

    // remaining whole words
    for (word_idx++; word_idx < words; word_idx++) {
        w = bs->data[word_idx];
        if (w != 0) {
            return (word_idx << 5) + rc_ctz_u32(w);
        }
    }

    return RC_INDEX_NONE;
}

/* ---- make_copy ---- */

/* A freshly allocated duplicate of src (same num).  The zero tail rides along, so the invariant holds. */
rc_bitset rc_bitset_make_copy(const rc_bitset *src, rc_arena *arena)
{
    rc_bitset out = {0};
    rc_bitset_resize(&out, src->num, arena);
    uint32_t words = words_for(src->num);
    if (words != 0) {
        memcpy(out.data, src->data, words * sizeof(uint32_t));
    }
    return out;
}

/* ---- copy ---- */

/* Assign src into dst in place.  The one equal-width op - a bare buffer copy, with no arena to grow into. */
void rc_bitset_copy(rc_bitset *dst, const rc_bitset *src)
{
    RC_ASSERT(dst->num == src->num);
    uint32_t words = words_for(src->num);
    if (words != 0) {
        memcpy(dst->data, src->data, words * sizeof(uint32_t));
    }
}

/* ---- union ---- */

/*
 * dst |= src, in place, capped to dst->num.  We OR the words the two share; a WIDER src may then have set bits
 * above dst->num sitting in the last shared word, so we re-mask dst's partial tail word to restore the
 * invariant (a no-op when src is no wider, or when dst is word-aligned).
 */
void rc_bitset_union(rc_bitset *dst, const rc_bitset *src)
{
    uint32_t dwords = words_for(dst->num);
    uint32_t swords = words_for(src->num);
    uint32_t shared = dwords < swords ? dwords : swords;
    for (uint32_t j = 0; j < shared; j++) {
        dst->data[j] |= src->data[j];
    }
    uint32_t bit = dst->num & 31u;
    if (bit != 0u) {
        dst->data[dst->num >> 5] &= (1u << bit) - 1u;
    }
}

/* ---- intersection ---- */

/*
 * dst &= src, in place, capped to dst->num.  Over each of dst's words we AND the matching src word, or 0 where
 * src has no word, so a NARROWER src correctly clears dst's upper bits.  AND never sets a bit, so the zero tail
 * survives untouched - no re-mask needed.
 */
void rc_bitset_intersection(rc_bitset *dst, const rc_bitset *src)
{
    uint32_t dwords = words_for(dst->num);
    uint32_t swords = words_for(src->num);
    for (uint32_t j = 0; j < dwords; j++) {
        dst->data[j] &= (j < swords) ? src->data[j] : 0u;
    }
}

/* ---- intersects ---- */

/*
 * Do the two sets share a set bit within the first min(a->num, b->num) bits?  Only the words both hold can
 * overlap; the zero tail keeps the boundary word exact (bits above the shorter set are 0, so never counted).
 */
bool rc_bitset_intersects(const rc_bitset *a, const rc_bitset *b)
{
    uint32_t awords = words_for(a->num);
    uint32_t bwords = words_for(b->num);
    uint32_t shared = awords < bwords ? awords : bwords;
    for (uint32_t j = 0; j < shared; j++) {
        if ((a->data[j] & b->data[j]) != 0u) {
            return true;
        }
    }
    return false;
}

/* ---- is_equal ---- */

/*
 * Identical sets?  A differing num is immediately not-equal; otherwise the layouts match and a whole-word
 * compare is exact - the zero tail compares equal on both sides.
 */
bool rc_bitset_is_equal(const rc_bitset *a, const rc_bitset *b)
{
    if (a->num != b->num) {
        return false;
    }
    uint32_t words = words_for(a->num);
    return words == 0 || memcmp(a->data, b->data, words * sizeof(uint32_t)) == 0;
}

/* ---- num_set_bits ---- */

/* Population count.  Summing whole-word popcounts is exact because every tail bit (>= num) is zero. */
uint32_t rc_bitset_num_set_bits(const rc_bitset *bs)
{
    uint32_t words = words_for(bs->num);
    uint32_t n = 0;
    for (uint32_t j = 0; j < words; j++) {
        n += rc_popcount_u32(bs->data[j]);
    }
    return n;
}
