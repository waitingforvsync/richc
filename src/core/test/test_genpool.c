#include "richc/test.h"

#define RC_GENPOOL_TYPE int
#include "richc/template/genpool.h"

// a second instantiation in the same TU, with a struct element type
typedef struct { int x, y; } rc_genpool_point_value;
#define RC_GENPOOL_TYPE rc_genpool_point_value
#define RC_GENPOOL_NAME rc_genpool_point
#include "richc/template/genpool.h"

RC_TEST_GROUP_DATA(genpool) {
    rc_arena a;
};

RC_TEST_GROUP_INIT(genpool, fix)
{
    fix->a = rc_arena_make_default();
}

RC_TEST_GROUP_DEINIT(genpool, fix)
{
    rc_arena_deinit(&fix->a);
}

// NB the free-side assertions (null handle, stale handle, double-free all trap
// in RC_GENPOOL_FREE_) are documented but untestable here: the test framework
// has no death-test facility, so only the non-trapping rejections (is_valid,
// at) are exercised.

RC_TEST(genpool, handle_functions)
{
    // a zero-initialised handle is the null handle
    rc_genpool_handle none = {0};
    RC_CHECK_TRUE(rc_genpool_handle_is_null(none));

    // make/index/gen round-trip (index 0 must survive the + 1 encoding)
    rc_genpool_handle h0 = rc_genpool_handle_make(0, 0);
    RC_CHECK_FALSE(rc_genpool_handle_is_null(h0));
    RC_CHECK(rc_genpool_handle_index(h0), ==, 0u);
    RC_CHECK(rc_genpool_handle_gen(h0), ==, 0u);

    rc_genpool_handle h = rc_genpool_handle_make(4, 7);
    RC_CHECK(rc_genpool_handle_index(h), ==, 4u);
    RC_CHECK(rc_genpool_handle_gen(h), ==, 7u);

    // equality requires both slot and generation to match
    RC_CHECK_TRUE(rc_genpool_handle_equal(h, rc_genpool_handle_make(4, 7)));
    RC_CHECK_FALSE(rc_genpool_handle_equal(h, rc_genpool_handle_make(5, 7)));
    RC_CHECK_FALSE(rc_genpool_handle_equal(h, rc_genpool_handle_make(4, 8)));
    RC_CHECK_TRUE(rc_genpool_handle_equal(none, (rc_genpool_handle) {0}));
    RC_CHECK_FALSE(rc_genpool_handle_equal(none, h0));
}

RC_TEST_STEP(genpool, alloc_sequential, fix)
{
    rc_genpool_int pool = rc_genpool_int_make(0, &fix->a);
    for (uint32_t i = 0; i < 3; i++) {
        rc_genpool_handle h = rc_genpool_int_alloc(&pool, &fix->a);
        RC_CHECK(rc_genpool_handle_index(h), ==, i);
        RC_CHECK(rc_genpool_handle_gen(h), ==, 0u);   // fresh slots start at generation 0
        RC_CHECK_TRUE(rc_genpool_int_is_valid(&pool, h));
    }
    RC_CHECK(pool.items.num, ==, 3u);
}

RC_TEST_STEP(genpool, get_set_at, fix)
{
    rc_genpool_int pool = rc_genpool_int_make(0, &fix->a);
    rc_genpool_handle h = rc_genpool_int_alloc(&pool, &fix->a);
    RC_CHECK(rc_genpool_int_get(&pool, h), ==, 0);     // alloc zeroes the slot
    rc_genpool_int_set(&pool, h, 42);
    RC_CHECK(rc_genpool_int_get(&pool, h), ==, 42);
    RC_CHECK(*rc_genpool_int_at(&pool, h), ==, 42);
    *rc_genpool_int_at(&pool, h) = 99;
    RC_CHECK(rc_genpool_int_get(&pool, h), ==, 99);
    RC_CHECK(*rc_genpool_int_at_const(&pool, h), ==, 99);
}

RC_TEST_STEP(genpool, null_handle, fix)
{
    rc_genpool_int pool = rc_genpool_int_make(0, &fix->a);
    rc_genpool_int_alloc(&pool, &fix->a);   // a live element, so rejection is not
                                            // just the empty pool's bounds check
    rc_genpool_handle none = {0};
    RC_CHECK_FALSE(rc_genpool_int_is_valid(&pool, none));
    RC_CHECK_TRUE(rc_genpool_int_at(&pool, none) == NULL);
    RC_CHECK_TRUE(rc_genpool_int_at_const(&pool, none) == NULL);
}

RC_TEST_STEP(genpool, stale_handle, fix)
{
    rc_genpool_int pool = rc_genpool_int_make(0, &fix->a);
    rc_genpool_handle h = rc_genpool_int_alloc(&pool, &fix->a);
    rc_genpool_int_set(&pool, h, 42);
    RC_CHECK_TRUE(rc_genpool_int_is_valid(&pool, h));

    // freeing the element stales the handle: the slot's generation moved on
    rc_genpool_int_free(&pool, h);
    RC_CHECK_FALSE(rc_genpool_int_is_valid(&pool, h));
    RC_CHECK_TRUE(rc_genpool_int_at(&pool, h) == NULL);
    RC_CHECK_TRUE(rc_genpool_int_at_const(&pool, h) == NULL);

    // a forged handle to a slot the pool never grew is rejected by the range check
    rc_genpool_handle forged = rc_genpool_handle_make(99, 0);
    RC_CHECK_FALSE(rc_genpool_int_is_valid(&pool, forged));
    RC_CHECK_TRUE(rc_genpool_int_at(&pool, forged) == NULL);

    // a forged handle to a live slot with the wrong generation is rejected too
    rc_genpool_handle wrong_gen = rc_genpool_handle_make(0, 99);
    RC_CHECK_FALSE(rc_genpool_int_is_valid(&pool, wrong_gen));
    RC_CHECK_TRUE(rc_genpool_int_at(&pool, wrong_gen) == NULL);
}

RC_TEST_STEP(genpool, gen_bump_on_reuse, fix)
{
    rc_genpool_int pool = rc_genpool_int_make(0, &fix->a);
    rc_genpool_handle h0 = rc_genpool_int_alloc(&pool, &fix->a);
    RC_CHECK(rc_genpool_handle_gen(h0), ==, 0u);

    // free then re-alloc: same slot, advanced generation, old handle stale
    rc_genpool_int_free(&pool, h0);
    rc_genpool_handle h1 = rc_genpool_int_alloc(&pool, &fix->a);
    RC_CHECK(rc_genpool_handle_index(h1), ==, rc_genpool_handle_index(h0));
    RC_CHECK(rc_genpool_handle_gen(h1), ==, 1u);
    RC_CHECK_FALSE(rc_genpool_int_is_valid(&pool, h0));
    RC_CHECK_TRUE(rc_genpool_int_is_valid(&pool, h1));
    RC_CHECK_FALSE(rc_genpool_handle_equal(h0, h1));

    // and again: each lifetime gets its own generation
    rc_genpool_int_free(&pool, h1);
    rc_genpool_handle h2 = rc_genpool_int_alloc(&pool, &fix->a);
    RC_CHECK(rc_genpool_handle_gen(h2), ==, 2u);
    RC_CHECK_FALSE(rc_genpool_int_is_valid(&pool, h0));
    RC_CHECK_FALSE(rc_genpool_int_is_valid(&pool, h1));
    RC_CHECK_TRUE(rc_genpool_int_is_valid(&pool, h2));
}

RC_TEST_STEP(genpool, free_reuse_middle, fix)
{
    rc_genpool_int pool = rc_genpool_int_make(0, &fix->a);
    rc_genpool_int_alloc(&pool, &fix->a);                       // 0
    rc_genpool_handle b = rc_genpool_int_alloc(&pool, &fix->a); // 1
    rc_genpool_int_alloc(&pool, &fix->a);                       // 2
    rc_genpool_int_set(&pool, b, 7);

    rc_genpool_int_free(&pool, b);              // middle slot -> onto the free list
    RC_CHECK(pool.items.num, ==, 3u);           // not popped
    rc_genpool_handle r = rc_genpool_int_alloc(&pool, &fix->a);
    RC_CHECK(rc_genpool_handle_index(r), ==, rc_genpool_handle_index(b));   // slot reused
    RC_CHECK(rc_genpool_int_get(&pool, r), ==, 0);   // reused slot is zeroed
}

RC_TEST_STEP(genpool, free_list_lifo, fix)
{
    rc_genpool_int pool = rc_genpool_int_make(0, &fix->a);
    rc_genpool_handle h[4];
    for (int i = 0; i < 4; i++) {
        h[i] = rc_genpool_int_alloc(&pool, &fix->a);   // 0,1,2,3
    }
    rc_genpool_int_free(&pool, h[1]);           // free list: 1
    rc_genpool_int_free(&pool, h[2]);           // free list: 2 -> 1
    RC_CHECK(pool.items.num, ==, 4u);
    // both freed slots are recycled LIFO: 2 was freed last, so it is reused first
    RC_CHECK(rc_genpool_handle_index(rc_genpool_int_alloc(&pool, &fix->a)), ==, 2u);
    RC_CHECK(rc_genpool_handle_index(rc_genpool_int_alloc(&pool, &fix->a)), ==, 1u);
    RC_CHECK(pool.items.num, ==, 4u);
}

RC_TEST_STEP(genpool, index_stability, fix)
{
    rc_genpool_int pool = rc_genpool_int_make(0, &fix->a);
    rc_genpool_handle a = rc_genpool_int_alloc(&pool, &fix->a);   // 0
    rc_genpool_handle b = rc_genpool_int_alloc(&pool, &fix->a);   // 1
    rc_genpool_int_set(&pool, b, 1234);
    int *pb = rc_genpool_int_at(&pool, b);

    // freeing another slot does not move b or grow the array, so b's handle and
    // pointer stay valid
    rc_genpool_int_free(&pool, a);
    RC_CHECK_TRUE(rc_genpool_int_is_valid(&pool, b));
    RC_CHECK(rc_genpool_int_get(&pool, b), ==, 1234);
    RC_CHECK_TRUE(pb == rc_genpool_int_at(&pool, b));
    RC_CHECK(*pb, ==, 1234);
}

RC_TEST_STEP(genpool, reset, fix)
{
    rc_genpool_int pool = rc_genpool_int_make(0, &fix->a);
    rc_genpool_int_alloc(&pool, &fix->a);
    rc_genpool_handle h = rc_genpool_int_alloc(&pool, &fix->a);
    rc_genpool_int_free(&pool, h);   // slot 1 now at generation 1
    rc_genpool_int_reset(&pool);
    RC_CHECK(pool.items.num, ==, 0u);
    RC_CHECK(pool.first_free, ==, 0u);   // empty free list

    // allocation starts over from slot 0 at generation 0 - which is why (as the
    // header documents) handles issued before a reset can alias handles issued
    // after it; the pre-reset handles are simply out of range until re-growth
    rc_genpool_handle r = rc_genpool_int_alloc(&pool, &fix->a);
    RC_CHECK(rc_genpool_handle_index(r), ==, 0u);
    RC_CHECK(rc_genpool_handle_gen(r), ==, 0u);
}

RC_TEST_STEP(genpool, deinit, fix)
{
    rc_genpool_int pool = rc_genpool_int_make(8, &fix->a);
    rc_genpool_int_alloc(&pool, &fix->a);
    rc_genpool_int_alloc(&pool, &fix->a);

    rc_genpool_int_deinit(&pool, &fix->a);
    RC_CHECK(pool.items.num, ==, 0u);
    RC_CHECK(pool.items.cap, ==, 0u);
    RC_CHECK_TRUE(pool.items.data == NULL);
    RC_CHECK(pool.first_free, ==, 0u);
    // the zeroed pool is usable again
    RC_CHECK(rc_genpool_handle_index(rc_genpool_int_alloc(&pool, &fix->a)), ==, 0u);
}

RC_TEST_STEP(genpool, zero_init, fix)
{
    // a zero-initialised pool is a valid empty pool (no make needed)
    rc_genpool_int pool = {0};
    rc_genpool_handle h = rc_genpool_int_alloc(&pool, &fix->a);
    RC_CHECK(rc_genpool_handle_index(h), ==, 0u);
    rc_genpool_int_set(&pool, h, 42);
    RC_CHECK(rc_genpool_int_get(&pool, h), ==, 42);
    // free slot 0 onto the list, then re-alloc it (index 0 must round-trip
    // through the + 1 free-list encoding)
    rc_genpool_int_alloc(&pool, &fix->a);   // 1
    rc_genpool_int_free(&pool, h);
    rc_genpool_handle r = rc_genpool_int_alloc(&pool, &fix->a);
    RC_CHECK(rc_genpool_handle_index(r), ==, 0u);
    RC_CHECK(rc_genpool_int_get(&pool, r), ==, 0);   // reused slot is zeroed
}

RC_TEST_STEP(genpool, growth_and_reuse, fix)
{
    rc_genpool_int pool = rc_genpool_int_make(0, &fix->a);

    // many allocs force the backing array to grow several times; handles are
    // stable so values are addressed by handle, never a cached pointer
    rc_genpool_handle h[100];
    for (uint32_t i = 0; i < 100; i++) {
        h[i] = rc_genpool_int_alloc(&pool, &fix->a);
        RC_CHECK(rc_genpool_handle_index(h[i]), ==, i);
        rc_genpool_int_set(&pool, h[i], (int)(i * 10));
    }
    for (uint32_t i = 0; i < 100; i++) {
        // generations survive the growth: every handle still validates
        RC_CHECK_TRUE(rc_genpool_int_is_valid(&pool, h[i]));
        RC_CHECK(rc_genpool_int_get(&pool, h[i]), ==, (int)(i * 10));
    }

    // free every even slot
    for (uint32_t i = 0; i < 100; i += 2) {
        rc_genpool_int_free(&pool, h[i]);
    }
    RC_CHECK(pool.items.num, ==, 100u);

    // re-alloc reuses the freed slots at generation 1, each zeroed; the old even
    // handles are stale and the odd entries untouched
    for (uint32_t i = 0; i < 50; i++) {
        rc_genpool_handle r = rc_genpool_int_alloc(&pool, &fix->a);
        RC_CHECK(rc_genpool_handle_index(r) % 2u, ==, 0u);
        RC_CHECK(rc_genpool_handle_gen(r), ==, 1u);
        RC_CHECK(rc_genpool_int_get(&pool, r), ==, 0);
    }
    for (uint32_t i = 0; i < 100; i++) {
        if (i % 2 == 0) {
            RC_CHECK_FALSE(rc_genpool_int_is_valid(&pool, h[i]));
        } else {
            RC_CHECK(rc_genpool_int_get(&pool, h[i]), ==, (int)(i * 10));
        }
    }
}

RC_TEST_STEP(genpool, free_bitset, fix)
{
    rc_genpool_int pool = rc_genpool_int_make(0, &fix->a);
    rc_genpool_handle h[8];
    for (uint32_t i = 0; i < 8; i++) {
        h[i] = rc_genpool_int_alloc(&pool, &fix->a);
    }
    rc_genpool_int_free(&pool, h[2]);
    rc_genpool_int_free(&pool, h[5]);

    rc_bitset dead = rc_genpool_int_free_bitset(&pool, &fix->a);
    RC_CHECK(dead.num, ==, 8u);
    for (uint32_t i = 0; i < 8; i++) {
        RC_CHECK(rc_bitset_is_set(&dead, i), ==, (i == 2 || i == 5));
    }
}

RC_TEST_STEP(genpool, handle_at, fix)
{
    rc_genpool_int pool = rc_genpool_int_make(0, &fix->a);
    rc_genpool_handle a = rc_genpool_int_alloc(&pool, &fix->a);   // 0, gen 0
    rc_genpool_handle b = rc_genpool_int_alloc(&pool, &fix->a);   // 1, gen 0
    rc_genpool_int_free(&pool, b);
    rc_genpool_handle b2 = rc_genpool_int_alloc(&pool, &fix->a);  // 1, gen 1

    // handle_at reconstructs exactly the handle each live slot validates against
    RC_CHECK_TRUE(rc_genpool_handle_equal(rc_genpool_int_handle_at(&pool, 0), a));
    RC_CHECK_TRUE(rc_genpool_handle_equal(rc_genpool_int_handle_at(&pool, 1), b2));
    RC_CHECK_FALSE(rc_genpool_handle_equal(rc_genpool_int_handle_at(&pool, 1), b));
}

RC_TEST_STEP(genpool, struct_elements, fix)
{
    rc_genpool_point pool = rc_genpool_point_make(0, &fix->a);
    rc_genpool_handle h = rc_genpool_point_alloc(&pool, &fix->a);
    rc_genpool_point_value zero = rc_genpool_point_get(&pool, h);
    RC_CHECK(zero.x, ==, 0);
    RC_CHECK(zero.y, ==, 0);

    rc_genpool_point_set(&pool, h, (rc_genpool_point_value) {.x = 3, .y = 4});
    rc_genpool_point_value p = rc_genpool_point_get(&pool, h);
    RC_CHECK(p.x, ==, 3);
    RC_CHECK(p.y, ==, 4);
    rc_genpool_point_at(&pool, h)->x = 5;
    RC_CHECK(rc_genpool_point_get(&pool, h).x, ==, 5);

    // the generation machinery works identically for struct elements
    rc_genpool_point_free(&pool, h);
    RC_CHECK_FALSE(rc_genpool_point_is_valid(&pool, h));
    RC_CHECK_TRUE(rc_genpool_point_at(&pool, h) == NULL);
    rc_genpool_handle r = rc_genpool_point_alloc(&pool, &fix->a);
    RC_CHECK(rc_genpool_handle_gen(r), ==, 1u);
}
