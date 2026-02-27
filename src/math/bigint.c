/*
 * math/bigint.c - rc_bigint implementation.
 *
 * Internal helpers
 * ----------------
 * limb_add_carry(a, b, carry)  — compute a + b + *carry, update *carry (0 or 1)
 * limb_sub_borrow(a, b, borrow) — compute a - b - *borrow, update *borrow (0 or 1)
 * bigint_normalize(a)          — strip leading zero limbs; set sign=0 if len==0
 * bigint_mag_cmp(a, b)         — compare unsigned magnitudes: -1 / 0 / +1
 *
 * Addition algorithm
 * ------------------
 * rc_bigint_add(a, b):
 *   Zero cases: if b==0 return; if a==0 copy b into a.
 *   Same sign: add magnitudes limb-by-limb with carry; result sign = a->sign.
 *     Reserve max(a->len, b->len)+1 limbs first so there is room for a carry
 *     ripple into a new most-significant limb.
 *   Different sign:
 *     |a| == |b|: result is zero.
 *     |a|  > |b|: subtract b from a in-place; result sign = a->sign.
 *     |a|  < |b|: compute b - a, store in a; result sign = b->sign.
 *       This case saves a_old_digits before reserve (the pointer remains valid
 *       in the arena even if realloc returns a new pointer), then subtracts
 *       a_old_digits from b->digits into a->digits.
 *
 * Aliasing notes
 * --------------
 * add(a, b) with a == b (self-add):
 *   Both pointers refer to the same struct so b->digits is always the current
 *   value of a->digits.  Since a->sign == b->sign always holds, the same-sign
 *   path is taken.  Each limb is read into a local (bd) before a->digits[i]
 *   is overwritten, so self-add is correct.
 *
 * add(a, b) different-sign, |b| > |a|:
 *   a != b is guaranteed (same struct implies same sign).  a_old_digits is
 *   captured before reserve; if reserve extends in-place a_old_digits ==
 *   a->digits, but each read still precedes the corresponding write.
 */

#include "richc/math/bigint.h"
#include <string.h>

/* ---- internal helpers ---- */

/*
 * Add two limbs plus an incoming carry (0 or 1).
 * Writes the outgoing carry (0 or 1) back to *carry and returns the sum.
 */
static uint64_t limb_add_carry(uint64_t a, uint64_t b, uint64_t *carry)
{
    uint64_t sum = a + b;
    uint64_t c1  = (sum < a);        /* overflow from a + b      */
    sum         += *carry;
    uint64_t c2  = (sum < *carry);   /* overflow from adding carry */
    *carry       = c1 | c2;
    return sum;
}

/*
 * Subtract b and an incoming borrow (0 or 1) from a.
 * Writes the outgoing borrow (0 or 1) back to *borrow and returns the difference.
 */
static uint64_t limb_sub_borrow(uint64_t a, uint64_t b, uint64_t *borrow)
{
    uint64_t diff = a - b;
    uint64_t b1   = (diff > a);        /* underflow from a - b        */
    uint64_t prev = diff;
    diff         -= *borrow;
    uint64_t b2   = (diff > prev);     /* underflow from subtracting borrow */
    *borrow       = b1 | b2;
    return diff;
}

/* Strip leading zero limbs.  Set sign to 0 if the result is zero. */
static void bigint_normalize(rc_bigint *a)
{
    while (a->len > 0 && a->digits[a->len - 1] == 0)
        a->len--;
    if (a->len == 0)
        a->sign = 0;
}

/*
 * Compare unsigned magnitudes.
 * Returns -1 if |a| < |b|, 0 if |a| == |b|, +1 if |a| > |b|.
 */
static int bigint_mag_cmp(const rc_bigint *a, const rc_bigint *b)
{
    if (a->len != b->len)
        return a->len > b->len ? 1 : -1;
    for (uint32_t i = a->len; i-- > 0; ) {
        if (a->digits[i] != b->digits[i])
            return a->digits[i] > b->digits[i] ? 1 : -1;
    }
    return 0;
}

/* ---- construction ---- */

rc_bigint rc_bigint_make(uint32_t cap, rc_arena *a)
{
    RC_ASSERT(a != NULL);
    if (cap < 8) cap = 8;
    uint64_t *digits = rc_arena_alloc_type(a, uint64_t, cap);
    return (rc_bigint) {.digits = digits, .len = 0, .cap = cap, .sign = 0};
}

rc_bigint rc_bigint_from_u64(uint64_t v, rc_arena *a)
{
    rc_bigint r = rc_bigint_make(8, a);
    if (v != 0) {
        r.digits[0] = v;
        r.len       = 1;
        r.sign      = 1;
    }
    return r;
}

rc_bigint rc_bigint_from_i64(int64_t v, rc_arena *a)
{
    rc_bigint r = rc_bigint_make(8, a);
    if (v != 0) {
        /*
         * (uint64_t)0 - (uint64_t)v gives the correct absolute value for all
         * negative int64_t, including INT64_MIN, via unsigned modular arithmetic.
         */
        r.digits[0] = v < 0 ? (uint64_t)0 - (uint64_t)v : (uint64_t)v;
        r.len       = 1;
        r.sign      = v > 0 ? 1 : -1;
    }
    return r;
}

rc_bigint rc_bigint_copy(const rc_bigint *src, rc_arena *a)
{
    RC_ASSERT(rc_bigint_is_valid(src));
    RC_ASSERT(a != NULL);
    uint32_t cap = src->cap < 8 ? 8 : src->cap;
    uint64_t *digits = rc_arena_alloc_type(a, uint64_t, cap);
    if (src->len > 0)
        memcpy(digits, src->digits, src->len * sizeof(uint64_t));
    return (rc_bigint) {
        .digits = digits,
        .len    = src->len,
        .cap    = cap,
        .sign   = src->sign
    };
}

/* ---- mutation ---- */

void rc_bigint_reserve(rc_bigint *a, uint32_t new_cap, rc_arena *arena)
{
    if (new_cap <= a->cap) return;
    RC_ASSERT(arena != NULL);
    uint32_t grown = a->cap < 8 ? 8 : a->cap * 2;
    if (grown < new_cap) grown = new_cap;
    a->digits = rc_arena_realloc(arena, a->digits,
                                 a->cap  * (uint32_t)sizeof(uint64_t),
                                 grown   * (uint32_t)sizeof(uint64_t));
    a->cap = grown;
}

void rc_bigint_add(rc_bigint *a, const rc_bigint *b, rc_arena *arena)
{
    RC_ASSERT(rc_bigint_is_valid(a));
    RC_ASSERT(rc_bigint_is_valid(b));

    if (b->sign == 0) return;

    if (a->sign == 0) {
        /* a is zero: copy b. */
        rc_bigint_reserve(a, b->len, arena);
        memcpy(a->digits, b->digits, b->len * sizeof(uint64_t));
        a->len  = b->len;
        a->sign = b->sign;
        return;
    }

    if (a->sign == b->sign) {
        /*
         * Same sign: add magnitudes with carry.
         *
         * Reserve enough room for a carry out of the most significant limb.
         * If b is longer, zero-extend a to b->len first.
         */
        uint32_t max_len = a->len > b->len ? a->len : b->len;
        rc_bigint_reserve(a, max_len + 1, arena);

        if (b->len > a->len) {
            memset(a->digits + a->len, 0,
                   (b->len - a->len) * sizeof(uint64_t));
            a->len = b->len;
        }

        uint64_t carry = 0;
        uint32_t i;
        for (i = 0; i < b->len; i++) {
            uint64_t bd  = b->digits[i];   /* read before overwriting a->digits[i] */
            a->digits[i] = limb_add_carry(a->digits[i], bd, &carry);
        }
        for (; carry && i < a->len; i++) {
            a->digits[i] = limb_add_carry(a->digits[i], 0, &carry);
        }
        if (carry) {
            a->digits[a->len] = 1;
            a->len++;
        }
    } else {
        /*
         * Different signs: subtract the smaller magnitude from the larger.
         * a != b is guaranteed here (same pointer implies same sign).
         */
        int cmp = bigint_mag_cmp(a, b);

        if (cmp == 0) {
            rc_bigint_reset(a);
            return;
        }

        if (cmp > 0) {
            /* |a| > |b|: a->digits -= b->digits in-place; sign stays. */
            uint64_t borrow = 0;
            uint32_t i;
            for (i = 0; i < b->len; i++) {
                uint64_t bd  = b->digits[i];
                a->digits[i] = limb_sub_borrow(a->digits[i], bd, &borrow);
            }
            for (; borrow && i < a->len; i++) {
                a->digits[i] = limb_sub_borrow(a->digits[i], 0, &borrow);
            }
            RC_ASSERT(borrow == 0);
        } else {
            /*
             * |b| > |a|: result = b - a, written into a's buffer.
             *
             * Save a's current pointer before reserve — the old arena region
             * remains readable even if reserve returns a new pointer, and the
             * reads of a_old_digits[i] happen before the writes to a->digits[i]
             * (which may be the same address when reserve extended in-place).
             */
            uint32_t  a_old_len    = a->len;
            uint64_t *a_old_digits = a->digits;
            rc_bigint_reserve(a, b->len, arena);

            uint64_t borrow = 0;
            uint32_t i;
            for (i = 0; i < a_old_len; i++) {
                uint64_t ad  = a_old_digits[i];   /* read before overwriting */
                a->digits[i] = limb_sub_borrow(b->digits[i], ad, &borrow);
            }
            for (; i < b->len; i++) {
                a->digits[i] = limb_sub_borrow(b->digits[i], 0, &borrow);
            }
            RC_ASSERT(borrow == 0);
            a->len  = b->len;
            a->sign = b->sign;
        }

        bigint_normalize(a);
    }
}

void rc_bigint_add3(rc_bigint *result, const rc_bigint *b, const rc_bigint *c,
                    rc_arena *arena)
{
    RC_ASSERT(rc_bigint_is_valid(result));
    RC_ASSERT(rc_bigint_is_valid(b));
    RC_ASSERT(rc_bigint_is_valid(c));

    if (result == b) {
        /* result already holds b's value: just add c. */
        rc_bigint_add(result, c, arena);
        return;
    }
    if (result == c) {
        /* result already holds c's value: just add b. */
        rc_bigint_add(result, b, arena);
        return;
    }

    /* Copy b into result (reusing result's buffer if large enough), then add c. */
    rc_bigint_reserve(result, b->len, arena);
    if (b->len > 0)
        memcpy(result->digits, b->digits, b->len * sizeof(uint64_t));
    result->len  = b->len;
    result->sign = b->sign;
    rc_bigint_add(result, c, arena);
}
