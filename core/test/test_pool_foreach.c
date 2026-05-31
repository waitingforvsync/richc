#include "richc/test.h"

#define RC_POOL_TYPE int
#include "richc/template/pool.h"

// context callback: record each visited element's value and pool index, in order
// (reaching the object through the pool + index, not a raw pointer)
typedef struct { int values[32]; uint32_t indices[32]; uint32_t count; int sum; } visit_ctx;
static void visit_record(visit_ctx *c, rc_pool_int *pool, uint32_t index)
{
    int v = rc_pool_int_get(pool, index);
    c->values[c->count]  = v;
    c->indices[c->count] = index;
    c->count++;
    c->sum += v;
}

#define RC_POOL_FOREACH_TYPE          int
#define RC_POOL_FOREACH_CTX           visit_ctx
#define RC_POOL_FOREACH_FUNC(c, p, i) visit_record(c, p, i)
#include "richc/template/pool_foreach.h"   // rc_pool_foreach_int

// non-context callback accumulating through file-scope state
static int      g_sum;
static uint32_t g_count;
static void sum_one(rc_pool_int *pool, uint32_t index) { g_sum += rc_pool_int_get(pool, index); g_count++; }

#define RC_POOL_FOREACH_TYPE       int
#define RC_POOL_FOREACH_FUNC(p, i) sum_one(p, i)
#define RC_POOL_FOREACH_NAME       rc_pool_foreach_int_sum
#include "richc/template/pool_foreach.h"

// non-context callback mutating the element in place, via the pool's at
#define RC_POOL_FOREACH_TYPE       int
#define RC_POOL_FOREACH_FUNC(p, i) (*rc_pool_int_at(p, i) *= 2)
#define RC_POOL_FOREACH_NAME       rc_pool_foreach_int_double
#include "richc/template/pool_foreach.h"

RC_TEST_GROUP_DATA(pool_foreach) {
    rc_arena a;
};

RC_TEST_GROUP_INIT(pool_foreach, fix)
{
    fix->a = rc_arena_make_default();
}

RC_TEST_GROUP_DEINIT(pool_foreach, fix)
{
    rc_arena_deinit(&fix->a);
}

// Fill a fresh pool with n live elements valued 0, 10, 20, ...
static rc_pool_int make_pool(uint32_t n, rc_arena *arena)
{
    rc_pool_int pool = {0};
    for (uint32_t i = 0; i < n; i++) {
        uint32_t idx = rc_pool_int_alloc(&pool, arena);
        rc_pool_int_set(&pool, idx, (int)(i * 10));
    }
    return pool;
}

RC_TEST_STEP(pool_foreach, empty, fix)
{
    rc_pool_int pool = {0};
    visit_ctx ctx = {0};
    rc_pool_foreach_int(&pool, &ctx, fix->a);
    RC_CHECK(ctx.count, ==, 0u);
}

RC_TEST_STEP(pool_foreach, all_live, fix)
{
    rc_pool_int pool = make_pool(10, &fix->a);
    visit_ctx ctx = {0};
    rc_pool_foreach_int(&pool, &ctx, fix->a);
    RC_CHECK(ctx.count, ==, 10u);
    RC_CHECK(ctx.sum, ==, 450);             // 0 + 10 + ... + 90
    for (uint32_t i = 0; i < 10; i++) {
        RC_CHECK(ctx.values[i], ==, (int)(i * 10));   // visited in index order
        RC_CHECK(ctx.indices[i], ==, i);              // the callback gets the slot index
    }
}

RC_TEST_STEP(pool_foreach, holes, fix)
{
    rc_pool_int pool = make_pool(10, &fix->a);
    rc_pool_int_free(&pool, 2);     // non-tail -> onto the free list
    rc_pool_int_free(&pool, 5);
    rc_pool_int_free(&pool, 7);

    visit_ctx ctx = {0};
    rc_pool_foreach_int(&pool, &ctx, fix->a);
    RC_CHECK(ctx.count, ==, 7u);
    int expect[]            = {0, 10, 30, 40, 60, 80, 90};
    uint32_t expect_idx[]   = {0, 1, 3, 4, 6, 8, 9};   // the live slots (2,5,7 freed)
    for (uint32_t i = 0; i < 7; i++) {
        RC_CHECK(ctx.values[i], ==, expect[i]);
        RC_CHECK(ctx.indices[i], ==, expect_idx[i]);
    }
    RC_CHECK(ctx.sum, ==, 0 + 10 + 30 + 40 + 60 + 80 + 90);
}

RC_TEST_STEP(pool_foreach, free_tail, fix)
{
    rc_pool_int pool = make_pool(10, &fix->a);
    rc_pool_int_free(&pool, 9);     // tail -> pops, num shrinks to 9
    rc_pool_int_free(&pool, 4);     // middle -> free list
    RC_CHECK(pool.items.num, ==, 9u);

    visit_ctx ctx = {0};
    rc_pool_foreach_int(&pool, &ctx, fix->a);
    RC_CHECK(ctx.count, ==, 8u);    // 0..8 minus 4
    int expect[] = {0, 10, 20, 30, 50, 60, 70, 80};
    for (uint32_t i = 0; i < 8; i++) RC_CHECK(ctx.values[i], ==, expect[i]);
}

RC_TEST_STEP(pool_foreach, non_context, fix)
{
    rc_pool_int pool = make_pool(5, &fix->a);   // values 0,10,20,30,40
    rc_pool_int_free(&pool, 1);                 // drop value 10

    g_sum = 0;
    g_count = 0;
    rc_pool_foreach_int_sum(&pool, fix->a);
    RC_CHECK(g_count, ==, 4u);
    RC_CHECK(g_sum, ==, 0 + 20 + 30 + 40);
}

RC_TEST_STEP(pool_foreach, mutate, fix)
{
    rc_pool_int pool = make_pool(6, &fix->a);   // values 0,10,20,30,40,50
    rc_pool_int_free(&pool, 3);                 // drop value 30

    rc_pool_foreach_int_double(&pool, fix->a);  // double each live element
    RC_CHECK(rc_pool_int_get(&pool, 0), ==, 0);
    RC_CHECK(rc_pool_int_get(&pool, 1), ==, 20);
    RC_CHECK(rc_pool_int_get(&pool, 2), ==, 40);
    RC_CHECK(rc_pool_int_get(&pool, 4), ==, 80);
    RC_CHECK(rc_pool_int_get(&pool, 5), ==, 100);
}

// Direct check of the pool.h helper that pool_foreach builds on.
RC_TEST_STEP(pool_foreach, free_bitset, fix)
{
    rc_pool_int pool = make_pool(8, &fix->a);
    rc_pool_int_free(&pool, 2);
    rc_pool_int_free(&pool, 5);

    rc_bitset dead = rc_pool_int_free_bitset(&pool, &fix->a);
    RC_CHECK(dead.num, ==, 8u);
    for (uint32_t i = 0; i < 8; i++) {
        RC_CHECK(rc_bitset_is_set(&dead, i), ==, (i == 2 || i == 5));
    }
}
