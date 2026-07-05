#include "richc/test.h"

// a plain map trie (hash == key)
#define RC_TRIE_KEY_TYPE   uint64_t
#define RC_TRIE_VALUE_TYPE int
#define RC_TRIE_HASH(k)    (k)
#define RC_TRIE_NAME       rc_trie_fe
#include "richc/template/hash_trie.h"

// constant hash: every key forms one deep chain, so iteration must descend through
// nested child blocks to reach them all
#define RC_TRIE_KEY_TYPE   uint64_t
#define RC_TRIE_VALUE_TYPE int
#define RC_TRIE_HASH(k)    0
#define RC_TRIE_NAME       rc_trie_fe_chain
#include "richc/template/hash_trie.h"

// context callback: count visits and sum keys + values (order-independent, since a
// trie visits in unspecified order), reaching each through key_get / value_get
typedef struct { uint32_t count; uint64_t key_sum; int val_sum; } visit_ctx;

static void visit_record(visit_ctx *c, rc_trie_fe *t, uint32_t index)
{
    c->count++;
    c->key_sum += rc_trie_fe_key_get(t, index);
    c->val_sum += rc_trie_fe_value_get(t, index);
}

#define RC_TRIE_FOREACH_TYPE          fe
#define RC_TRIE_FOREACH_CTX           visit_ctx
#define RC_TRIE_FOREACH_FUNC(c, t, i) visit_record(c, t, i)
#include "richc/template/algorithm/hash_trie_foreach.h"   // rc_trie_foreach_fe

static void visit_record_chain(visit_ctx *c, rc_trie_fe_chain *t, uint32_t index)
{
    c->count++;
    c->key_sum += rc_trie_fe_chain_key_get(t, index);
    c->val_sum += rc_trie_fe_chain_value_get(t, index);
}

#define RC_TRIE_FOREACH_TYPE          fe_chain
#define RC_TRIE_FOREACH_CTX           visit_ctx
#define RC_TRIE_FOREACH_FUNC(c, t, i) visit_record_chain(c, t, i)
#include "richc/template/algorithm/hash_trie_foreach.h"   // rc_trie_foreach_fe_chain

// non-context callback accumulating through file-scope state, with a name override
// so it does not clash with the default rc_trie_foreach_fe above
static uint32_t g_count;
static void count_one(rc_trie_fe *t, uint32_t index) { (void)t; (void)index; g_count++; }

#define RC_TRIE_FOREACH_TYPE       fe
#define RC_TRIE_FOREACH_FUNC(t, i) count_one(t, i)
#define RC_TRIE_FOREACH_NAME       rc_trie_foreach_fe_count
#include "richc/template/algorithm/hash_trie_foreach.h"

RC_TEST_GROUP_DATA(hash_trie_foreach) {
    rc_arena a;
};

RC_TEST_GROUP_INIT(hash_trie_foreach, fix)
{
    fix->a = rc_arena_make_default();
}

RC_TEST_GROUP_DEINIT(hash_trie_foreach, fix)
{
    rc_arena_deinit(&fix->a);
}

// Fill a fresh trie with keys 1..n, each valued key*10.  The pool is caller-owned
// (a trie handle only holds a pool*), so it must outlive the returned trie.
static rc_trie_fe make_trie(rc_trie_fe_pool *pool, uint64_t n, rc_arena *arena)
{
    rc_trie_fe t = rc_trie_fe_make(pool, arena);
    for (uint64_t k = 1; k <= n; k++) {
        rc_trie_fe_add(&t, k, (int)(k * 10), arena);
    }
    return t;
}

RC_TEST_STEP(hash_trie_foreach, empty, fix)
{
    rc_trie_fe_pool pool = rc_trie_fe_pool_make(0, &fix->a);
    rc_trie_fe t = make_trie(&pool, 0, &fix->a);
    visit_ctx ctx = {0};
    rc_trie_foreach_fe(&t, &ctx);
    RC_CHECK(ctx.count, ==, 0u);
}

RC_TEST_STEP(hash_trie_foreach, single, fix)
{
    rc_trie_fe_pool pool = rc_trie_fe_pool_make(0, &fix->a);
    rc_trie_fe t = make_trie(&pool, 1, &fix->a);
    visit_ctx ctx = {0};
    rc_trie_foreach_fe(&t, &ctx);
    RC_CHECK(ctx.count, ==, 1u);
    RC_CHECK(ctx.key_sum, ==, (uint64_t)1);
    RC_CHECK(ctx.val_sum, ==, 10);
}

RC_TEST_STEP(hash_trie_foreach, all_visited, fix)
{
    // 100 keys force several child blocks and a pool reallocation; every entry must
    // be visited exactly once
    rc_trie_fe_pool pool = rc_trie_fe_pool_make(0, &fix->a);
    rc_trie_fe t = make_trie(&pool, 100, &fix->a);
    visit_ctx ctx = {0};
    rc_trie_foreach_fe(&t, &ctx);
    RC_CHECK(ctx.count, ==, 100u);
    RC_CHECK(ctx.key_sum, ==, (uint64_t)(100 * 101 / 2));   // sum 1..100
    RC_CHECK(ctx.val_sum, ==, 100 * 101 / 2 * 10);
}

RC_TEST_STEP(hash_trie_foreach, deep_chain, fix)
{
    // every key hashes to 0, so the 20 entries form one 20-deep chain of child
    // blocks; the walk must recurse all the way down
    rc_trie_fe_chain_pool pool = rc_trie_fe_chain_pool_make(0, &fix->a);
    rc_trie_fe_chain t = rc_trie_fe_chain_make(&pool, &fix->a);
    for (uint64_t k = 1; k <= 20; k++) {
        rc_trie_fe_chain_add(&t, k, (int)k, &fix->a);
    }
    visit_ctx ctx = {0};
    rc_trie_foreach_fe_chain(&t, &ctx);
    RC_CHECK(ctx.count, ==, 20u);
    RC_CHECK(ctx.key_sum, ==, (uint64_t)(20 * 21 / 2));
    RC_CHECK(ctx.val_sum, ==, 20 * 21 / 2);
}

RC_TEST_STEP(hash_trie_foreach, skips_deleted, fix)
{
    // delete the even keys before iterating; only the odds should be visited
    rc_trie_fe_pool pool = rc_trie_fe_pool_make(0, &fix->a);
    rc_trie_fe t = make_trie(&pool, 10, &fix->a);
    for (uint64_t k = 2; k <= 10; k += 2) {
        rc_trie_fe_delete(&t, k);
    }
    visit_ctx ctx = {0};
    rc_trie_foreach_fe(&t, &ctx);
    RC_CHECK(ctx.count, ==, 5u);
    RC_CHECK(ctx.key_sum, ==, (uint64_t)(1 + 3 + 5 + 7 + 9));
    RC_CHECK(ctx.val_sum, ==, (1 + 3 + 5 + 7 + 9) * 10);
}

RC_TEST_STEP(hash_trie_foreach, non_context, fix)
{
    rc_trie_fe_pool pool = rc_trie_fe_pool_make(0, &fix->a);
    rc_trie_fe t = make_trie(&pool, 7, &fix->a);
    g_count = 0;
    rc_trie_foreach_fe_count(&t);
    RC_CHECK(g_count, ==, 7u);
}
