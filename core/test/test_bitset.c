#include "richc/arena.h"
#include "richc/bitset.h"
#include "richc/test.h"

RC_TEST_GROUP_DATA(bitset) {
    rc_arena a;
};

RC_TEST_GROUP_INIT(bitset, fix)
{
    fix->a = rc_arena_make_default();
}

RC_TEST_GROUP_DEINIT(bitset, fix)
{
    rc_arena_destroy(&fix->a);
}

/* ---- zero init ---- */

RC_TEST(bitset, zero_init)
{
    // a zero-initialised bitset is a valid empty bitset
    rc_bitset bs = {0};
    RC_CHECK(bs.num, ==, 0u);
    RC_CHECK(bs.cap, ==, 0u);
    RC_CHECK_TRUE(bs.data == NULL);
    RC_CHECK(rc_bitset_get_first_set(&bs), ==, RC_INDEX_NONE);
    // reset on an empty bitset is a no-op (must not deref NULL)
    rc_bitset_reset(&bs);
    RC_CHECK(bs.num, ==, 0u);
}

/* ---- reserve ---- */

RC_TEST_STEP(bitset, reserve_rounds_to_word, fix)
{
    rc_bitset bs = {0};
    rc_bitset_reserve(&bs, 1, &fix->a);
    RC_CHECK(bs.cap, ==, 32u);        // rounded up to a whole word

    rc_bitset_reserve(&bs, 33, &fix->a);
    RC_CHECK(bs.cap, ==, 64u);

    rc_bitset_reserve(&bs, 64, &fix->a);
    RC_CHECK(bs.cap, ==, 64u);        // exact multiple stays put

    rc_bitset_reserve(&bs, 65, &fix->a);
    RC_CHECK(bs.cap, ==, 96u);
}

RC_TEST_STEP(bitset, reserve_noop_when_smaller, fix)
{
    rc_bitset bs = {0};
    rc_bitset_reserve(&bs, 100, &fix->a);
    uint32_t cap = bs.cap;
    // a reserve that fits is a no-op and needs no arena
    rc_bitset_reserve(&bs, 50, NULL);
    RC_CHECK(bs.cap, ==, cap);
}

/* ---- set / clear / is_set ---- */

RC_TEST_STEP(bitset, set_clear_is_set, fix)
{
    rc_bitset bs = {0};
    rc_bitset_resize(&bs, 100, &fix->a);
    for (uint32_t i = 0; i < 100; i++) {
        RC_CHECK_FALSE(rc_bitset_is_set(&bs, i));
    }

    rc_bitset_set(&bs, 0);
    rc_bitset_set(&bs, 31);    // top of word 0
    rc_bitset_set(&bs, 32);    // bottom of word 1
    rc_bitset_set(&bs, 63);
    rc_bitset_set(&bs, 99);
    RC_CHECK_TRUE(rc_bitset_is_set(&bs, 0));
    RC_CHECK_TRUE(rc_bitset_is_set(&bs, 31));
    RC_CHECK_TRUE(rc_bitset_is_set(&bs, 32));
    RC_CHECK_TRUE(rc_bitset_is_set(&bs, 63));
    RC_CHECK_TRUE(rc_bitset_is_set(&bs, 99));
    RC_CHECK_FALSE(rc_bitset_is_set(&bs, 30));
    RC_CHECK_FALSE(rc_bitset_is_set(&bs, 33));

    rc_bitset_clear(&bs, 31);
    RC_CHECK_FALSE(rc_bitset_is_set(&bs, 31));
    RC_CHECK_TRUE(rc_bitset_is_set(&bs, 32));   // neighbour untouched
}

/* ---- iteration ---- */

RC_TEST_STEP(bitset, get_first_set, fix)
{
    rc_bitset bs = {0};
    rc_bitset_resize(&bs, 200, &fix->a);
    RC_CHECK(rc_bitset_get_first_set(&bs), ==, RC_INDEX_NONE);

    rc_bitset_set(&bs, 130);
    rc_bitset_set(&bs, 7);
    RC_CHECK(rc_bitset_get_first_set(&bs), ==, 7u);
    rc_bitset_clear(&bs, 7);
    RC_CHECK(rc_bitset_get_first_set(&bs), ==, 130u);
}

RC_TEST_STEP(bitset, get_next_set_iterates, fix)
{
    rc_bitset bs = {0};
    rc_bitset_resize(&bs, 256, &fix->a);

    // bits chosen to straddle word boundaries and a partial first word
    uint32_t bits[] = {0, 1, 31, 32, 63, 64, 100, 200, 255};
    uint32_t n = (uint32_t)(sizeof(bits) / sizeof(bits[0]));
    for (uint32_t i = 0; i < n; i++) {
        rc_bitset_set(&bs, bits[i]);
    }

    uint32_t k = 0;
    for (uint32_t i = rc_bitset_get_first_set(&bs);
         i != RC_INDEX_NONE;
         i = rc_bitset_get_next_set(&bs, i + 1)) {
        RC_CHECK(i, ==, bits[k]);
        k++;
    }
    RC_CHECK(k, ==, n);
}

RC_TEST_STEP(bitset, get_next_set_from_pos, fix)
{
    rc_bitset bs = {0};
    rc_bitset_resize(&bs, 64, &fix->a);
    rc_bitset_set(&bs, 5);
    rc_bitset_set(&bs, 40);

    // pos exactly on a set bit returns that bit
    RC_CHECK(rc_bitset_get_next_set(&bs, 5), ==, 5u);
    RC_CHECK(rc_bitset_get_next_set(&bs, 6), ==, 40u);
    RC_CHECK(rc_bitset_get_next_set(&bs, 40), ==, 40u);
    RC_CHECK(rc_bitset_get_next_set(&bs, 41), ==, RC_INDEX_NONE);
    // pos >= num is out of range
    RC_CHECK(rc_bitset_get_next_set(&bs, 64), ==, RC_INDEX_NONE);
    RC_CHECK(rc_bitset_get_next_set(&bs, 1000), ==, RC_INDEX_NONE);
}

/* ---- resize ---- */

RC_TEST_STEP(bitset, resize_grow_keeps_bits, fix)
{
    rc_bitset bs = {0};
    rc_bitset_resize(&bs, 40, &fix->a);
    rc_bitset_set(&bs, 10);
    rc_bitset_set(&bs, 39);

    rc_bitset_resize(&bs, 200, &fix->a);
    RC_CHECK(bs.num, ==, 200u);
    RC_CHECK_TRUE(rc_bitset_is_set(&bs, 10));
    RC_CHECK_TRUE(rc_bitset_is_set(&bs, 39));
    // freshly grown bits are zero
    RC_CHECK(rc_bitset_get_next_set(&bs, 40), ==, RC_INDEX_NONE);
}

RC_TEST_STEP(bitset, resize_shrink_clears_vacated, fix)
{
    rc_bitset bs = {0};
    rc_bitset_resize(&bs, 200, &fix->a);
    rc_bitset_set(&bs, 5);
    rc_bitset_set(&bs, 50);     // survives: below new_num
    rc_bitset_set(&bs, 70);     // vacated: in the partial word straddling new_num
    rc_bitset_set(&bs, 150);    // vacated: in a full word above new_num

    rc_bitset_resize(&bs, 66, &fix->a);
    RC_CHECK(bs.num, ==, 66u);
    RC_CHECK_TRUE(rc_bitset_is_set(&bs, 5));
    RC_CHECK_TRUE(rc_bitset_is_set(&bs, 50));
    // grow back: the vacated bits must read as zero (invariant restored)
    rc_bitset_resize(&bs, 200, &fix->a);
    RC_CHECK_TRUE(rc_bitset_is_set(&bs, 5));
    RC_CHECK_TRUE(rc_bitset_is_set(&bs, 50));
    RC_CHECK_FALSE(rc_bitset_is_set(&bs, 70));
    RC_CHECK_FALSE(rc_bitset_is_set(&bs, 150));
    RC_CHECK(rc_bitset_get_next_set(&bs, 51), ==, RC_INDEX_NONE);
}

RC_TEST_STEP(bitset, resize_shrink_word_aligned, fix)
{
    // shrinking to a word boundary takes the "no partial word" path
    rc_bitset bs = {0};
    rc_bitset_resize(&bs, 128, &fix->a);
    rc_bitset_set(&bs, 10);
    rc_bitset_set(&bs, 64);
    rc_bitset_set(&bs, 100);

    rc_bitset_resize(&bs, 64, &fix->a);
    rc_bitset_resize(&bs, 128, &fix->a);
    RC_CHECK_TRUE(rc_bitset_is_set(&bs, 10));
    RC_CHECK_FALSE(rc_bitset_is_set(&bs, 64));
    RC_CHECK_FALSE(rc_bitset_is_set(&bs, 100));
}

/* ---- reset ---- */

RC_TEST_STEP(bitset, reset_keeps_capacity, fix)
{
    rc_bitset bs = {0};
    rc_bitset_resize(&bs, 100, &fix->a);
    rc_bitset_set(&bs, 3);
    rc_bitset_set(&bs, 99);
    uint32_t cap = bs.cap;

    rc_bitset_reset(&bs);
    RC_CHECK(bs.num, ==, 100u);     // num unchanged
    RC_CHECK(bs.cap, ==, cap);      // cap unchanged
    RC_CHECK(rc_bitset_get_first_set(&bs), ==, RC_INDEX_NONE);
}

/* ---- push ---- */

RC_TEST_STEP(bitset, push, fix)
{
    rc_bitset bs = {0};
    RC_CHECK(rc_bitset_push(&bs, true, &fix->a), ==, 0u);
    RC_CHECK(rc_bitset_push(&bs, false, &fix->a), ==, 1u);
    RC_CHECK(rc_bitset_push(&bs, true, &fix->a), ==, 2u);
    RC_CHECK(bs.num, ==, 3u);
    RC_CHECK_TRUE(rc_bitset_is_set(&bs, 0));
    RC_CHECK_FALSE(rc_bitset_is_set(&bs, 1));
    RC_CHECK_TRUE(rc_bitset_is_set(&bs, 2));
}

RC_TEST_STEP(bitset, push_geometric_floor, fix)
{
    // the first growth jumps straight to the 64-bit floor
    rc_bitset bs = {0};
    rc_bitset_push(&bs, true, &fix->a);
    RC_CHECK(bs.cap, ==, 64u);
}

RC_TEST_STEP(bitset, push_many_across_words, fix)
{
    rc_bitset bs = {0};
    // set odd indices, clear even ones, across several word boundaries
    for (uint32_t i = 0; i < 300; i++) {
        uint32_t idx = rc_bitset_push(&bs, (i & 1u) != 0u, &fix->a);
        RC_CHECK(idx, ==, i);
    }
    RC_CHECK(bs.num, ==, 300u);
    for (uint32_t i = 0; i < 300; i++) {
        RC_CHECK(rc_bitset_is_set(&bs, i), ==, (i & 1u) != 0u);
    }
}

RC_TEST_STEP(bitset, push_n_zero, fix)
{
    rc_bitset bs = {0};
    rc_bitset_push(&bs, true, &fix->a);          // 0
    RC_CHECK(rc_bitset_push_n_zero(&bs, 70, &fix->a), ==, 1u);
    RC_CHECK(bs.num, ==, 71u);
    RC_CHECK_TRUE(rc_bitset_is_set(&bs, 0));
    // every appended bit is zero
    RC_CHECK(rc_bitset_get_next_set(&bs, 1), ==, RC_INDEX_NONE);

    // setting within the new range then pushing more keeps the invariant
    rc_bitset_set(&bs, 70);
    RC_CHECK(rc_bitset_push(&bs, false, &fix->a), ==, 71u);
    RC_CHECK_TRUE(rc_bitset_is_set(&bs, 70));
    RC_CHECK_FALSE(rc_bitset_is_set(&bs, 71));
}

RC_TEST_STEP(bitset, push_n_zero_empty, fix)
{
    // pushing zero bits is a no-op that still reports the current end index
    rc_bitset bs = {0};
    rc_bitset_push_n_zero(&bs, 5, &fix->a);
    RC_CHECK(rc_bitset_push_n_zero(&bs, 0, &fix->a), ==, 5u);
    RC_CHECK(bs.num, ==, 5u);
}
