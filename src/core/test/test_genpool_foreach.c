#include "richc/test.h"

#define RC_GENPOOL_TYPE int
#include "richc/template/genpool.h"

// context callback: record each visited element's value and slot index, in order,
// and check the handle validates (reaching the object through the pool + handle,
// not a raw pointer)
typedef struct { int values[32]; uint32_t indices[32]; uint32_t count; int sum; bool all_valid; } visit_ctx;
static void visit_record(visit_ctx *c, rc_genpool_int *pool, rc_genpool_handle handle)
{
    c->all_valid = c->all_valid && rc_genpool_int_is_valid(pool, handle);
    int v = rc_genpool_int_get(pool, handle);
    c->values[c->count]  = v;
    c->indices[c->count] = rc_genpool_handle_index(handle);
    c->count++;
    c->sum += v;
}

#define RC_GENPOOL_FOREACH_POOL          rc_genpool_int
#define RC_GENPOOL_FOREACH_CTX           visit_ctx
#define RC_GENPOOL_FOREACH_FUNC(c, p, h) visit_record(c, p, h)
#include "richc/template/algorithm/genpool_foreach.h"   // rc_genpool_int_foreach

// non-context callback accumulating through file-scope state
static int      g_sum;
static uint32_t g_count;
static void sum_one(rc_genpool_int *pool, rc_genpool_handle handle) { g_sum += rc_genpool_int_get(pool, handle); g_count++; }

#define RC_GENPOOL_FOREACH_POOL       rc_genpool_int
#define RC_GENPOOL_FOREACH_FUNC(p, h) sum_one(p, h)
#define RC_GENPOOL_FOREACH_NAME       rc_genpool_int_foreach_sum
#include "richc/template/algorithm/genpool_foreach.h"

// non-context callback mutating the element in place, via the pool's at
#define RC_GENPOOL_FOREACH_POOL       rc_genpool_int
#define RC_GENPOOL_FOREACH_FUNC(p, h) (*rc_genpool_int_at(p, h) *= 2)
#define RC_GENPOOL_FOREACH_NAME       rc_genpool_int_foreach_double
#include "richc/template/algorithm/genpool_foreach.h"

RC_TEST_GROUP_DATA(genpool_foreach) {
    rc_arena a;
};

RC_TEST_GROUP_INIT(genpool_foreach, fix)
{
    fix->a = rc_arena_make_default();
}

RC_TEST_GROUP_DEINIT(genpool_foreach, fix)
{
    rc_arena_deinit(&fix->a);
}

// Fill a fresh pool with n live elements valued 0, 10, 20, ..., recording the
// handles into out (when non-NULL)
static rc_genpool_int make_pool(uint32_t n, rc_genpool_handle *out, rc_arena *arena)
{
    rc_genpool_int pool = {0};
    for (uint32_t i = 0; i < n; i++) {
        rc_genpool_handle h = rc_genpool_int_alloc(&pool, arena);
        rc_genpool_int_set(&pool, h, (int)(i * 10));
        if (out) {
            out[i] = h;
        }
    }
    return pool;
}

RC_TEST_STEP(genpool_foreach, empty, fix)
{
    rc_genpool_int pool = {0};
    visit_ctx ctx = {.all_valid = true};
    rc_genpool_int_foreach(&pool, &ctx, fix->a);
    RC_CHECK(ctx.count, ==, 0u);
}

RC_TEST_STEP(genpool_foreach, all_live, fix)
{
    rc_genpool_int pool = make_pool(10, NULL, &fix->a);
    visit_ctx ctx = {.all_valid = true};
    rc_genpool_int_foreach(&pool, &ctx, fix->a);
    RC_CHECK(ctx.count, ==, 10u);
    RC_CHECK(ctx.sum, ==, 450);             // 0 + 10 + ... + 90
    RC_CHECK_TRUE(ctx.all_valid);           // every callback handle validates
    for (uint32_t i = 0; i < 10; i++) {
        RC_CHECK(ctx.values[i], ==, (int)(i * 10));   // visited in index order
        RC_CHECK(ctx.indices[i], ==, i);              // the handle carries the slot index
    }
}

RC_TEST_STEP(genpool_foreach, holes, fix)
{
    rc_genpool_handle h[10];
    rc_genpool_int pool = make_pool(10, h, &fix->a);
    rc_genpool_int_free(&pool, h[2]);   // non-tail -> onto the free list
    rc_genpool_int_free(&pool, h[5]);
    rc_genpool_int_free(&pool, h[7]);

    visit_ctx ctx = {.all_valid = true};
    rc_genpool_int_foreach(&pool, &ctx, fix->a);
    RC_CHECK(ctx.count, ==, 7u);
    RC_CHECK_TRUE(ctx.all_valid);
    int expect[]          = {0, 10, 30, 40, 60, 80, 90};
    uint32_t expect_idx[] = {0, 1, 3, 4, 6, 8, 9};   // the live slots (2,5,7 freed)
    for (uint32_t i = 0; i < 7; i++) {
        RC_CHECK(ctx.values[i], ==, expect[i]);
        RC_CHECK(ctx.indices[i], ==, expect_idx[i]);
    }
    RC_CHECK(ctx.sum, ==, 0 + 10 + 30 + 40 + 60 + 80 + 90);
}

RC_TEST_STEP(genpool_foreach, free_tail, fix)
{
    rc_genpool_handle h[10];
    rc_genpool_int pool = make_pool(10, h, &fix->a);
    rc_genpool_int_free(&pool, h[9]);   // tail -> free list (num is not shrunk)
    rc_genpool_int_free(&pool, h[4]);   // middle -> free list
    RC_CHECK(pool.items.num, ==, 10u);

    // foreach must skip the freed tail slot 9 even though it is still within num
    visit_ctx ctx = {.all_valid = true};
    rc_genpool_int_foreach(&pool, &ctx, fix->a);
    RC_CHECK(ctx.count, ==, 8u);    // 0..8 minus 4, and 9 is freed
    int expect[] = {0, 10, 20, 30, 50, 60, 70, 80};
    for (uint32_t i = 0; i < 8; i++) RC_CHECK(ctx.values[i], ==, expect[i]);
}

RC_TEST_STEP(genpool_foreach, reused_slots, fix)
{
    rc_genpool_handle h[6];
    rc_genpool_int pool = make_pool(6, h, &fix->a);   // values 0,10,20,30,40,50
    rc_genpool_int_free(&pool, h[1]);
    rc_genpool_int_free(&pool, h[4]);
    rc_genpool_handle r4 = rc_genpool_int_alloc(&pool, &fix->a);   // slot 4, gen 1
    rc_genpool_handle r1 = rc_genpool_int_alloc(&pool, &fix->a);   // slot 1, gen 1
    rc_genpool_int_set(&pool, r4, 44);
    rc_genpool_int_set(&pool, r1, 11);

    // the callback handles carry the bumped generations: all validate (checked in
    // visit_record) and the reused slots show their new values
    visit_ctx ctx = {.all_valid = true};
    rc_genpool_int_foreach(&pool, &ctx, fix->a);
    RC_CHECK(ctx.count, ==, 6u);
    RC_CHECK_TRUE(ctx.all_valid);
    int expect[] = {0, 11, 20, 30, 44, 50};
    for (uint32_t i = 0; i < 6; i++) {
        RC_CHECK(ctx.values[i], ==, expect[i]);
        RC_CHECK(ctx.indices[i], ==, i);
    }
}

RC_TEST_STEP(genpool_foreach, non_context, fix)
{
    rc_genpool_handle h[5];
    rc_genpool_int pool = make_pool(5, h, &fix->a);   // values 0,10,20,30,40
    rc_genpool_int_free(&pool, h[1]);                 // drop value 10

    g_sum = 0;
    g_count = 0;
    rc_genpool_int_foreach_sum(&pool, fix->a);
    RC_CHECK(g_count, ==, 4u);
    RC_CHECK(g_sum, ==, 0 + 20 + 30 + 40);
}

RC_TEST_STEP(genpool_foreach, mutate, fix)
{
    rc_genpool_handle h[6];
    rc_genpool_int pool = make_pool(6, h, &fix->a);   // values 0,10,20,30,40,50
    rc_genpool_int_free(&pool, h[3]);                 // drop value 30

    rc_genpool_int_foreach_double(&pool, fix->a);     // double each live element
    RC_CHECK(rc_genpool_int_get(&pool, h[0]), ==, 0);
    RC_CHECK(rc_genpool_int_get(&pool, h[1]), ==, 20);
    RC_CHECK(rc_genpool_int_get(&pool, h[2]), ==, 40);
    RC_CHECK(rc_genpool_int_get(&pool, h[4]), ==, 80);
    RC_CHECK(rc_genpool_int_get(&pool, h[5]), ==, 100);
}
