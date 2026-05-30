#include "richc/test.h"

#define RC_POOL_TYPE int
#include "richc/template/pool.h"

// a second instantiation in the same TU, with a struct element type
typedef struct { int x, y; } rc_pool_point_value;
#define RC_POOL_TYPE rc_pool_point_value
#define RC_POOL_NAME rc_pool_point
#include "richc/template/pool.h"

RC_TEST_GROUP_DATA(pool) {
    rc_arena a;
};

RC_TEST_GROUP_INIT(pool, fix)
{
    fix->a = rc_arena_make_default();
}

RC_TEST_GROUP_DEINIT(pool, fix)
{
    rc_arena_destroy(&fix->a);
}

RC_TEST_STEP(pool, alloc_sequential, fix)
{
    rc_pool_int pool = rc_pool_int_make(0, &fix->a);
    RC_CHECK(rc_pool_int_alloc(&pool, &fix->a), ==, 0u);
    RC_CHECK(rc_pool_int_alloc(&pool, &fix->a), ==, 1u);
    RC_CHECK(rc_pool_int_alloc(&pool, &fix->a), ==, 2u);
    RC_CHECK(pool.items.num, ==, 3u);
}

RC_TEST_STEP(pool, get_set_at, fix)
{
    rc_pool_int pool = rc_pool_int_make(0, &fix->a);
    uint32_t i = rc_pool_int_alloc(&pool, &fix->a);
    RC_CHECK(rc_pool_int_get(&pool, i), ==, 0);     // alloc zeroes the slot
    rc_pool_int_set(&pool, i, 42);
    RC_CHECK(rc_pool_int_get(&pool, i), ==, 42);
    RC_CHECK(*rc_pool_int_at(&pool, i), ==, 42);
    *rc_pool_int_at(&pool, i) = 99;
    RC_CHECK(rc_pool_int_get(&pool, i), ==, 99);
}

RC_TEST_STEP(pool, free_reuse_middle, fix)
{
    rc_pool_int pool = rc_pool_int_make(0, &fix->a);
    rc_pool_int_alloc(&pool, &fix->a);          // 0
    uint32_t b = rc_pool_int_alloc(&pool, &fix->a);   // 1
    rc_pool_int_alloc(&pool, &fix->a);          // 2
    rc_pool_int_set(&pool, b, 7);

    rc_pool_int_free(&pool, b);                 // middle slot -> onto the free list
    RC_CHECK(pool.items.num, ==, 3u);           // not popped
    uint32_t r = rc_pool_int_alloc(&pool, &fix->a);
    RC_CHECK(r, ==, b);                          // index reused
    RC_CHECK(rc_pool_int_get(&pool, r), ==, 0);  // reused slot is zeroed
}

RC_TEST_STEP(pool, free_pop_trailing, fix)
{
    rc_pool_int pool = rc_pool_int_make(0, &fix->a);
    rc_pool_int_alloc(&pool, &fix->a);          // 0
    rc_pool_int_alloc(&pool, &fix->a);          // 1
    rc_pool_int_alloc(&pool, &fix->a);          // 2
    RC_CHECK(pool.items.num, ==, 3u);

    // freeing a non-last slot leaves the backing length unchanged
    rc_pool_int_free(&pool, 1);
    RC_CHECK(pool.items.num, ==, 3u);
    // freeing the last slot pops it
    rc_pool_int_free(&pool, 2);
    RC_CHECK(pool.items.num, ==, 2u);

    // the popped tail is re-appended on the next non-recycled alloc... but slot 1
    // is still on the free list, so the next alloc reuses it first
    RC_CHECK(rc_pool_int_alloc(&pool, &fix->a), ==, 1u);
}

RC_TEST_STEP(pool, free_list_lifo, fix)
{
    rc_pool_int pool = rc_pool_int_make(0, &fix->a);
    for (int i = 0; i < 4; i++) {
        rc_pool_int_alloc(&pool, &fix->a);      // 0,1,2,3
    }
    rc_pool_int_free(&pool, 1);                 // free list: 1
    rc_pool_int_free(&pool, 2);                 // free list: 2 -> 1
    RC_CHECK(pool.items.num, ==, 4u);
    RC_CHECK(rc_pool_int_alloc(&pool, &fix->a), ==, 2u);   // LIFO
    RC_CHECK(rc_pool_int_alloc(&pool, &fix->a), ==, 1u);
}

RC_TEST_STEP(pool, index_stability, fix)
{
    rc_pool_int pool = rc_pool_int_make(0, &fix->a);
    uint32_t a = rc_pool_int_alloc(&pool, &fix->a);   // 0
    uint32_t b = rc_pool_int_alloc(&pool, &fix->a);   // 1
    rc_pool_int_set(&pool, b, 1234);
    int *pb = rc_pool_int_at(&pool, b);

    // freeing another slot does not move b or grow the array, so b's index and
    // pointer stay valid
    rc_pool_int_free(&pool, a);
    RC_CHECK(rc_pool_int_get(&pool, b), ==, 1234);
    RC_CHECK_TRUE(pb == rc_pool_int_at(&pool, b));
    RC_CHECK(*pb, ==, 1234);
}

RC_TEST_STEP(pool, reset, fix)
{
    rc_pool_int pool = rc_pool_int_make(0, &fix->a);
    rc_pool_int_alloc(&pool, &fix->a);
    rc_pool_int_alloc(&pool, &fix->a);
    rc_pool_int_reset(&pool);
    RC_CHECK(pool.items.num, ==, 0u);
    RC_CHECK(pool.first_free, ==, 0u);   // empty free list
    // allocation starts over from index 0
    RC_CHECK(rc_pool_int_alloc(&pool, &fix->a), ==, 0u);
}

RC_TEST_STEP(pool, zero_init, fix)
{
    // a zero-initialised pool is a valid empty pool (no make needed)
    rc_pool_int pool = {0};
    RC_CHECK(rc_pool_int_alloc(&pool, &fix->a), ==, 0u);
    rc_pool_int_set(&pool, 0, 42);
    RC_CHECK(rc_pool_int_get(&pool, 0), ==, 42);
    // free index 0 onto the list, then re-alloc it (index 0 must round-trip
    // through the + 1 free-list encoding)
    rc_pool_int_alloc(&pool, &fix->a);   // 1, so freeing 0 is not a trailing pop
    rc_pool_int_free(&pool, 0);
    RC_CHECK(rc_pool_int_alloc(&pool, &fix->a), ==, 0u);
    RC_CHECK(rc_pool_int_get(&pool, 0), ==, 0);   // reused slot is zeroed
}

RC_TEST_STEP(pool, deinit, fix)
{
    rc_pool_int pool = rc_pool_int_make(8, &fix->a);
    rc_pool_int_alloc(&pool, &fix->a);
    rc_pool_int_alloc(&pool, &fix->a);

    rc_pool_int_deinit(&pool, &fix->a);
    RC_CHECK(pool.items.num, ==, 0u);
    RC_CHECK(pool.items.cap, ==, 0u);
    RC_CHECK_TRUE(pool.items.data == NULL);
    RC_CHECK(pool.first_free, ==, 0u);
    // the zeroed pool is usable again
    RC_CHECK(rc_pool_int_alloc(&pool, &fix->a), ==, 0u);
}

RC_TEST_STEP(pool, growth_and_reuse, fix)
{
    rc_pool_int pool = rc_pool_int_make(0, &fix->a);

    // many allocs force the backing array to grow several times; indices are
    // stable so values are addressed by index, never a cached pointer
    for (uint32_t i = 0; i < 100; i++) {
        uint32_t idx = rc_pool_int_alloc(&pool, &fix->a);
        RC_CHECK(idx, ==, i);
        rc_pool_int_set(&pool, idx, (int)(i * 10));
    }
    for (uint32_t i = 0; i < 100; i++) {
        RC_CHECK(rc_pool_int_get(&pool, i), ==, (int)(i * 10));
    }

    // free every even index (index 99 is odd and stays live, so nothing pops)
    for (uint32_t i = 0; i < 100; i += 2) {
        rc_pool_int_free(&pool, i);
    }
    RC_CHECK(pool.items.num, ==, 100u);

    // re-alloc reuses the freed slots, each zeroed; odd entries are untouched
    for (uint32_t i = 0; i < 50; i++) {
        uint32_t idx = rc_pool_int_alloc(&pool, &fix->a);
        RC_CHECK(idx % 2u, ==, 0u);
        RC_CHECK(rc_pool_int_get(&pool, idx), ==, 0);
    }
    for (uint32_t i = 1; i < 100; i += 2) {
        RC_CHECK(rc_pool_int_get(&pool, i), ==, (int)(i * 10));
    }
}

RC_TEST_STEP(pool, struct_elements, fix)
{
    rc_pool_point pool = rc_pool_point_make(0, &fix->a);
    uint32_t i = rc_pool_point_alloc(&pool, &fix->a);
    rc_pool_point_value zero = rc_pool_point_get(&pool, i);
    RC_CHECK(zero.x, ==, 0);
    RC_CHECK(zero.y, ==, 0);

    rc_pool_point_set(&pool, i, (rc_pool_point_value) {.x = 3, .y = 4});
    rc_pool_point_value p = rc_pool_point_get(&pool, i);
    RC_CHECK(p.x, ==, 3);
    RC_CHECK(p.y, ==, 4);
    rc_pool_point_at(&pool, i)->x = 5;
    RC_CHECK(rc_pool_point_get(&pool, i).x, ==, 5);
}
