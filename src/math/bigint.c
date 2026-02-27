/*
 * math/bigint.c - rc_bigint implementation.
 *
 * Internal helpers
 * ----------------
 * limb_add_carry(a, b, carry)   — a + b + *carry via uint64_t; updates *carry
 * limb_sub_borrow(a, b, borrow) — a - b - *borrow via uint64_t; updates *borrow
 * bigint_normalize(a)           — strip leading zero limbs; set sign=0 if len==0
 *
 * Limb type: uint32_t.  Using 32-bit limbs means carry and borrow arithmetic
 * can be done exactly in uint64_t without any overflow extension tricks:
 *   add:  (uint64_t)a + b + carry  — max value 2^33-1, fits in uint64_t
 *   sub:  (uint64_t)a - b - borrow — wraps; sign detected via bit 63
 *   mul:  (uint64_t)a * b + acc + carry — max 2^64-1, fits in uint64_t
 *
 * Unified add/sub kernel: bigint_add_impl
 * ----------------------------------------
 * Both rc_bigint_add and rc_bigint_sub are thin wrappers around
 * bigint_add_impl(result, b, c, c_sign_factor, arena).
 * The function computes result = b + c * c_sign_factor.
 *
 * All input data (lengths, signs, digit pointers) are captured as local
 * variables before any rc_bigint_reserve call.  Loop bodies read from the
 * captured pointers into local limb variables before writing to
 * result->digits[i].  This handles all aliasing (result==b, result==c,
 * b==c, result==b==c) uniformly with no branch logic.
 *
 * Dispatch:
 *   c_eff == 0               → result = b  (memmove-safe copy).
 *   b->sign == 0             → result = c * c_sign_factor.
 *   b->sign == c_eff         → add magnitudes with carry; sign unchanged.
 *   b->sign != c_eff         → subtract smaller magnitude from larger:
 *       |b| == |c|: reset result to zero.
 *       |b|  > |c|: result = b - c; sign = b->sign.
 *       |b|  < |c|: result = c - b; sign = c_eff.
 *
 * Multiplication kernel: bigint_mul_magnitudes
 * ---------------------------------------------
 * Schoolbook O(m*n) algorithm.  Each pair of 32-bit limbs is multiplied
 * into a 64-bit accumulator: (uint64_t)a[i] * b[j] + acc + carry.
 * The maximum value of that expression is (2^32-1)^2 + (2^32-1) + (2^32-1)
 * = 2^64 - 1, which fits exactly in uint64_t.
 *
 * rc_bigint_mul always computes the product into a temporary buffer in the
 * scratch arena (passed by value, discarded on return), then copies the
 * result into the output buffer.  b->digits and c->digits are read before
 * any rc_bigint_reserve call, so aliasing (result==b, result==c) is safe.
 *
 * Division: bigint_div1 / bigint_div_knuth
 * -----------------------------------------
 * Single-limb divisor: simple long-division loop using uint64_t.
 * Multi-limb divisor: Knuth Vol.2 Algorithm D (base B = 2^32).
 *   D1. Normalise: shift both operands left by d = clz(v[n-1]) bits so the
 *       top bit of the divisor is set.  un has u_len+1 limbs, vn has n limbs.
 *   D2-D7. For j = m..0: estimate q[j] using the top two limbs of the
 *       partial dividend, refine with a two-step test, then multiply-subtract.
 *       Add back if the estimate was 1 too large (happens rarely).
 *   D8. Unnormalise the remainder by shifting right d bits.
 *
 * rc_bigint_divmod captures all source data (lengths, signs, digit pointers)
 * before any output writes, so all aliasing combinations of quot/rem/b/c are
 * safe (provided quot != rem, which is asserted).
 */

#include "richc/math/bigint.h"
#include "richc/math/math.h"
#include <string.h>

/* ---- internal helpers ---- */

/*
 * Add two 32-bit limbs plus an incoming carry (0 or 1).
 * Returns the 32-bit sum; writes the outgoing carry (0 or 1) to *carry.
 */
static uint32_t limb_add_carry(uint32_t a, uint32_t b, uint32_t *carry)
{
    uint64_t sum = (uint64_t)a + b + *carry;
    *carry = (uint32_t)(sum >> 32);
    return (uint32_t)sum;
}

/*
 * Subtract b and an incoming borrow (0 or 1) from a.
 * Returns the 32-bit difference; writes the outgoing borrow (0 or 1) to *borrow.
 */
static uint32_t limb_sub_borrow(uint32_t a, uint32_t b, uint32_t *borrow)
{
    uint64_t diff = (uint64_t)a - b - *borrow;
    *borrow = (uint32_t)(diff >> 63) & 1;   /* 1 iff underflowed */
    return (uint32_t)diff;
}

/* Strip leading zero limbs.  Set sign to 0 if the result is zero. */
static void bigint_normalize(rc_bigint *a)
{
    while (a->len > 0 && a->digits[a->len - 1] == 0)
        a->len--;
    if (a->len == 0)
        a->sign = 0;
}

/* ---- construction ---- */

rc_bigint rc_bigint_make(uint32_t cap, rc_arena *a)
{
    RC_ASSERT(a != NULL);
    if (cap < 8) cap = 8;
    uint32_t *digits = rc_arena_alloc_type(a, uint32_t, cap);
    return (rc_bigint) {.digits = digits, .len = 0, .cap = cap, .sign = 0};
}

rc_bigint rc_bigint_from_u64(uint64_t v, rc_arena *a)
{
    rc_bigint r = rc_bigint_make(0, a);
    if (v == 0) return r;
    r.sign      = 1;
    r.digits[0] = (uint32_t)v;
    r.len       = 1;
    if (v > UINT32_MAX) {
        r.digits[1] = (uint32_t)(v >> 32);
        r.len       = 2;
    }
    return r;
}

rc_bigint rc_bigint_from_i64(int64_t v, rc_arena *a)
{
    if (v == 0) return rc_bigint_make(0, a);
    /*
     * (uint64_t)0 - (uint64_t)v gives the correct absolute value for all
     * negative int64_t, including INT64_MIN, via unsigned modular arithmetic.
     */
    uint64_t  mag = v < 0 ? (uint64_t)0 - (uint64_t)v : (uint64_t)v;
    rc_bigint r   = rc_bigint_from_u64(mag, a);
    r.sign        = v < 0 ? -1 : 1;
    return r;
}

rc_bigint rc_bigint_copy(const rc_bigint *src, rc_arena *a)
{
    RC_ASSERT(rc_bigint_is_valid(src));
    RC_ASSERT(a != NULL);
    uint32_t  cap    = src->cap < 8 ? 8 : src->cap;
    uint32_t *digits = rc_arena_alloc_type(a, uint32_t, cap);
    if (src->len > 0)
        memcpy(digits, src->digits, src->len * sizeof(uint32_t));
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
                                 a->cap * (uint32_t)sizeof(uint32_t),
                                 grown  * (uint32_t)sizeof(uint32_t));
    a->cap = grown;
}

/*
 * Core of add and sub.  Computes result = b + c * c_sign_factor.
 * c_sign_factor is +1 for addition, -1 for subtraction.
 *
 * All input data is captured before any rc_bigint_reserve call so that
 * any aliasing between result, b, and c is handled correctly.
 */
static void bigint_add_impl(rc_bigint *result,
                             const rc_bigint *b, const rc_bigint *c,
                             int32_t c_sign_factor, rc_arena *arena)
{
    /* 1. Capture all inputs. */
    int32_t         c_eff = c->sign * c_sign_factor;
    int32_t         bs    = b->sign;
    uint32_t        bl    = b->len,  cl = c->len;
    const uint32_t *bd    = b->digits, *cd = c->digits;

    /* 2. Zero cases: copy one operand to result (memmove handles aliasing). */
    if (c_eff == 0) {
        rc_bigint_reserve(result, bl, arena);
        memmove(result->digits, bd, bl * sizeof(uint32_t));
        result->len  = bl;
        result->sign = bs;
        return;
    }
    if (bs == 0) {
        rc_bigint_reserve(result, cl, arena);
        memmove(result->digits, cd, cl * sizeof(uint32_t));
        result->len  = cl;
        result->sign = c_eff;
        return;
    }

    /* 3. Same effective sign: add magnitudes with carry. */
    if (bs == c_eff) {
        uint32_t max_len = bl > cl ? bl : cl;
        uint32_t min_len = bl < cl ? bl : cl;
        rc_bigint_reserve(result, max_len + 1, arena);

        uint32_t carry = 0;
        uint32_t i;
        for (i = 0; i < min_len; i++) {
            uint32_t bv = bd[i], cv = cd[i];
            result->digits[i] = limb_add_carry(bv, cv, &carry);
        }
        for (; i < bl; i++) {
            uint32_t bv = bd[i];
            result->digits[i] = limb_add_carry(bv, 0, &carry);
        }
        for (; i < cl; i++) {
            uint32_t cv = cd[i];
            result->digits[i] = limb_add_carry(0, cv, &carry);
        }
        if (carry) {
            result->digits[i] = 1;
            i++;
        }
        result->len  = i;
        result->sign = bs;
        return;
    }

    /* 4. Different effective signs: subtract smaller magnitude from larger.
     *    Inline magnitude comparison on the captured arrays. */
    int mag_cmp;
    if (bl != cl) {
        mag_cmp = bl > cl ? 1 : -1;
    } else {
        mag_cmp = 0;
        for (uint32_t i = bl; i-- > 0; ) {
            if (bd[i] != cd[i]) {
                mag_cmp = bd[i] > cd[i] ? 1 : -1;
                break;
            }
        }
    }

    if (mag_cmp == 0) {
        rc_bigint_reset(result);
        return;
    }

    if (mag_cmp > 0) {
        /* |b| > |c|: result = b - c, sign = bs. */
        rc_bigint_reserve(result, bl, arena);
        uint32_t borrow = 0;
        uint32_t i;
        for (i = 0; i < cl; i++) {
            uint32_t bv = bd[i], cv = cd[i];
            result->digits[i] = limb_sub_borrow(bv, cv, &borrow);
        }
        for (; i < bl; i++) {
            uint32_t bv = bd[i];
            result->digits[i] = limb_sub_borrow(bv, 0, &borrow);
        }
        RC_ASSERT(borrow == 0);
        result->len  = bl;
        result->sign = bs;
    } else {
        /* |c| > |b|: result = c - b, sign = c_eff. */
        rc_bigint_reserve(result, cl, arena);
        uint32_t borrow = 0;
        uint32_t i;
        for (i = 0; i < bl; i++) {
            uint32_t bv = bd[i], cv = cd[i];
            result->digits[i] = limb_sub_borrow(cv, bv, &borrow);
        }
        for (; i < cl; i++) {
            uint32_t cv = cd[i];
            result->digits[i] = limb_sub_borrow(cv, 0, &borrow);
        }
        RC_ASSERT(borrow == 0);
        result->len  = cl;
        result->sign = c_eff;
    }

    bigint_normalize(result);
}

void rc_bigint_add(rc_bigint *result, const rc_bigint *b, const rc_bigint *c,
                   rc_arena *arena)
{
    RC_ASSERT(rc_bigint_is_valid(result));
    RC_ASSERT(rc_bigint_is_valid(b));
    RC_ASSERT(rc_bigint_is_valid(c));
    bigint_add_impl(result, b, c, +1, arena);
}

void rc_bigint_sub(rc_bigint *result, const rc_bigint *b, const rc_bigint *c,
                   rc_arena *arena)
{
    RC_ASSERT(rc_bigint_is_valid(result));
    RC_ASSERT(rc_bigint_is_valid(b));
    RC_ASSERT(rc_bigint_is_valid(c));
    bigint_add_impl(result, b, c, -1, arena);
}

/*
 * Schoolbook multiply of two magnitudes into a pre-allocated, zeroed buffer.
 * res must have room for a_len + b_len limbs.
 * Returns the normalised length (leading zeros stripped).
 */
static uint32_t bigint_mul_magnitudes(uint32_t *res,
                                       const uint32_t *a, uint32_t a_len,
                                       const uint32_t *b, uint32_t b_len)
{
    uint32_t res_len = a_len + b_len;
    memset(res, 0, res_len * sizeof(uint32_t));
    for (uint32_t i = 0; i < a_len; i++) {
        uint64_t carry = 0;
        for (uint32_t j = 0; j < b_len; j++) {
            uint64_t t = (uint64_t)a[i] * b[j] + res[i + j] + carry;
            res[i + j] = (uint32_t)t;
            carry      = t >> 32;
        }
        res[i + b_len] = (uint32_t)carry;
    }
    while (res_len > 0 && res[res_len - 1] == 0)
        res_len--;
    return res_len;
}

void rc_bigint_mul(rc_bigint *result, const rc_bigint *b, const rc_bigint *c,
                   rc_arena *arena, rc_arena scratch)
{
    RC_ASSERT(rc_bigint_is_valid(result));
    RC_ASSERT(rc_bigint_is_valid(b));
    RC_ASSERT(rc_bigint_is_valid(c));

    if (b->sign == 0 || c->sign == 0) {
        rc_bigint_reset(result);
        return;
    }

    int32_t  sign = b->sign * c->sign;
    uint32_t cap  = b->len + c->len;

    /*
     * Always compute into scratch; copy to result afterward.
     * b->digits / c->digits are read by bigint_mul_magnitudes before
     * rc_bigint_reserve(result, ...) is called, so result may alias b or c.
     */
    rc_bigint tmp;
    tmp.digits = rc_arena_alloc_type(&scratch, uint32_t, cap);
    tmp.cap    = cap;
    tmp.len    = bigint_mul_magnitudes(tmp.digits, b->digits, b->len,
                                                   c->digits, c->len);
    tmp.sign   = tmp.len > 0 ? sign : 0;

    rc_bigint_reserve(result, tmp.len, arena);
    if (tmp.len > 0)
        memcpy(result->digits, tmp.digits, tmp.len * sizeof(uint32_t));
    result->len  = tmp.len;
    result->sign = tmp.sign;
}

/* ---- division helpers ---- */

/*
 * Shift n-limb src left by d bits (0..31); writes n+1 limbs to dst.
 * dst[n] holds the overflow from the top limb.
 */
static void limb_shl(uint32_t *dst, const uint32_t *src, uint32_t n, uint32_t d)
{
    if (d == 0) {
        memcpy(dst, src, n * sizeof(uint32_t));
        dst[n] = 0;
        return;
    }
    uint32_t carry = 0;
    for (uint32_t i = 0; i < n; i++) {
        dst[i] = (src[i] << d) | carry;
        carry  = src[i] >> (32 - d);
    }
    dst[n] = carry;
}

/*
 * Shift the first n limbs of arr right by d bits (0..31) in place.
 */
static void limb_shr(uint32_t *arr, uint32_t n, uint32_t d)
{
    if (d == 0 || n == 0) return;
    for (uint32_t i = 0; i + 1 < n; i++)
        arr[i] = (arr[i] >> d) | (arr[i + 1] << (32 - d));
    arr[n - 1] >>= d;
}

/*
 * un[off..off+n-1] -= q_hat * vn[0..n-1].
 * Returns the borrow that would propagate into un[off+n].
 */
static uint32_t limb_mul_sub(uint32_t *un, const uint32_t *vn,
                              uint32_t n, uint32_t q_hat, uint32_t off)
{
    uint64_t carry = 0;
    for (uint32_t i = 0; i < n; i++) {
        uint64_t prod = (uint64_t)q_hat * vn[i] + carry;
        carry         = prod >> 32;
        uint32_t lo   = (uint32_t)prod;
        if (un[off + i] < lo) carry++;
        un[off + i] -= lo;
    }
    return (uint32_t)carry;
}

/*
 * un[off..off+n-1] += vn[0..n-1].  Returns carry out.
 */
static uint32_t limb_add_partial(uint32_t *un, const uint32_t *vn,
                                 uint32_t n, uint32_t off)
{
    uint32_t carry = 0;
    for (uint32_t i = 0; i < n; i++) {
        uint64_t sum   = (uint64_t)un[off + i] + vn[i] + carry;
        un[off + i]    = (uint32_t)sum;
        carry          = (uint32_t)(sum >> 32);
    }
    return carry;
}

/*
 * Long-divide u[0..u_len-1] by single limb d.
 * Writes quotient to q[0..u_len-1].  Returns remainder.
 */
static uint32_t bigint_div1(uint32_t *q, const uint32_t *u,
                             uint32_t u_len, uint32_t d)
{
    uint64_t rem = 0;
    for (int32_t i = (int32_t)u_len - 1; i >= 0; i--) {
        uint64_t cur = (rem << 32) | u[i];
        q[i] = (uint32_t)(cur / d);
        rem  = cur % d;
    }
    return (uint32_t)rem;
}

/*
 * Knuth Vol.2 Algorithm D (base B = 2^32).
 *
 * Divides u[0..u_len-1] by v[0..v_len-1] where v_len >= 2 and v[v_len-1] != 0.
 * u_len >= v_len (caller guarantees |u| >= |v|, strictly greater here).
 *
 * scratch_buf: must hold (u_len+1) + (v_len+1) limbs.
 * Writes quotient digits to q[0..u_len-v_len] and remainder to r[0..v_len-1].
 * Caller normalises lengths and assigns signs.
 */
static void bigint_div_knuth(uint32_t *q, uint32_t *r,
                              const uint32_t *u, uint32_t u_len,
                              const uint32_t *v, uint32_t v_len,
                              uint32_t *scratch_buf)
{
    uint32_t n   = v_len;
    uint32_t m   = u_len - n;
    uint32_t d   = rc_clz_u32(v[n - 1]);  /* D1: normalisation shift */
    uint32_t *un = scratch_buf;            /* u_len+1 limbs */
    uint32_t *vn = scratch_buf + u_len + 1;/* v_len+1 limbs (vn[n] always 0) */

    limb_shl(un, u, u_len, d);
    limb_shl(vn, v, n, d);

    uint32_t vn1 = vn[n - 1];
    uint32_t vn2 = vn[n - 2];  /* n >= 2 guaranteed by caller */

    for (int32_t j = (int32_t)m; j >= 0; j--) {
        /* D3: estimate q̂. */
        uint64_t top2  = ((uint64_t)un[j + n] << 32) | un[j + n - 1];
        uint64_t q_hat = top2 / vn1;
        uint64_t r_hat = top2 % vn1;

        /* Refine estimate: at most two iterations. */
        while (q_hat > (uint64_t)UINT32_MAX ||
               q_hat * vn2 > (r_hat << 32) + un[j + n - 2]) {
            q_hat--;
            r_hat += vn1;
            if (r_hat > (uint64_t)UINT32_MAX) break;
        }

        /* D4-D5: multiply-subtract; add back if estimate was 1 too large. */
        uint32_t borrow = limb_mul_sub(un, vn, n, (uint32_t)q_hat, (uint32_t)j);
        if (borrow > un[j + n]) {
            q_hat--;
            uint32_t carry = limb_add_partial(un, vn, n, (uint32_t)j);
            un[j + n] += carry;
        }
        un[j + n] -= borrow;
        q[j] = (uint32_t)q_hat;
    }

    /* D8: unnormalise remainder. */
    limb_shr(un, n, d);
    memcpy(r, un, n * sizeof(uint32_t));
}

/* ---- division public API ---- */

void rc_bigint_divmod(rc_bigint *quot, rc_bigint *rem,
                      const rc_bigint *b, const rc_bigint *c,
                      rc_arena *arena, rc_arena scratch)
{
    RC_ASSERT(rc_bigint_is_valid(quot));
    RC_ASSERT(rc_bigint_is_valid(rem));
    RC_ASSERT(rc_bigint_is_valid(b));
    RC_ASSERT(rc_bigint_is_valid(c));
    RC_ASSERT(!rc_bigint_is_zero(c));

    /* Capture all inputs before any output writes. */
    int32_t         b_sign = b->sign, c_sign = c->sign;
    uint32_t        b_len  = b->len,  c_len  = c->len;
    const uint32_t *bd     = b->digits, *cd = c->digits;

    if (b_sign == 0) {
        rc_bigint_reset(quot);
        rc_bigint_reset(rem);
        return;
    }

    /* Inline magnitude comparison on captured arrays. */
    int mag;
    if (b_len != c_len) {
        mag = b_len > c_len ? 1 : -1;
    } else {
        mag = 0;
        for (uint32_t i = b_len; i-- > 0; ) {
            if (bd[i] != cd[i]) {
                mag = bd[i] > cd[i] ? 1 : -1;
                break;
            }
        }
    }

    if (mag < 0) {
        /* |b| < |c|: quot = 0, rem = b. */
        rc_bigint_reset(quot);
        rc_bigint_reserve(rem, b_len, arena);
        memmove(rem->digits, bd, b_len * sizeof(uint32_t));
        rem->len  = b_len;
        rem->sign = b_sign;
        return;
    }

    if (mag == 0) {
        /* |b| == |c|: quot = ±1, rem = 0. */
        rc_bigint_reset(rem);
        rc_bigint_reserve(quot, 1, arena);
        quot->digits[0] = 1;
        quot->len  = 1;
        quot->sign = b_sign * c_sign;
        return;
    }

    int32_t  quot_sign = b_sign * c_sign;
    int32_t  rem_sign  = b_sign;
    uint32_t m         = b_len - c_len;

    rc_bigint_reserve(quot, m + 1, arena);
    rc_bigint_reserve(rem,  c_len, arena);

    if (c_len == 1) {
        uint32_t r1 = bigint_div1(quot->digits, bd, b_len, cd[0]);
        quot->len  = b_len;
        bigint_normalize(quot);
        quot->sign = quot->len > 0 ? quot_sign : 0;
        rem->digits[0] = r1;
        rem->len  = r1 ? 1 : 0;
        rem->sign = rem->len > 0 ? rem_sign : 0;
    } else {
        /* Knuth D: scratch needs (b_len+1) + (c_len+1) limbs. */
        uint32_t *sbuf = rc_arena_alloc_type(&scratch, uint32_t,
                                             b_len + 1 + c_len + 1);
        bigint_div_knuth(quot->digits, rem->digits, bd, b_len, cd, c_len, sbuf);
        quot->len  = m + 1;
        bigint_normalize(quot);
        quot->sign = quot->len > 0 ? quot_sign : 0;
        rem->len   = c_len;
        bigint_normalize(rem);
        rem->sign  = rem->len > 0 ? rem_sign : 0;
    }
}

void rc_bigint_div(rc_bigint *result, const rc_bigint *b, const rc_bigint *c,
                   rc_arena *arena, rc_arena scratch)
{
    /* Allocate a temporary remainder in scratch; it never needs to grow. */
    rc_bigint rem = rc_bigint_make(c->len, &scratch);
    rc_bigint_divmod(result, &rem, b, c, arena, scratch);
}

void rc_bigint_mod(rc_bigint *result, const rc_bigint *b, const rc_bigint *c,
                   rc_arena *arena, rc_arena scratch)
{
    /* Allocate a temporary quotient in scratch; cap >= m+1, never grows. */
    uint32_t q_cap = (b->len >= c->len) ? (b->len - c->len + 1) : 1;
    rc_bigint quot = rc_bigint_make(q_cap, &scratch);
    rc_bigint_divmod(&quot, result, b, c, arena, scratch);
}
