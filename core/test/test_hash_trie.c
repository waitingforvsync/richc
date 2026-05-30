#include "richc/test.h"

// identity hash: lets the tests craft exact collision keys
#define RC_TRIE_KEY_TYPE   uint64_t
#define RC_TRIE_VALUE_TYPE int
#define RC_TRIE_HASH(k)    (k)
#define RC_TRIE_NAME       rc_trie_test
#include "richc/template/hash_trie.h"

// constant hash: forces every key into one deep O(n) chain
#define RC_TRIE_KEY_TYPE   uint64_t
#define RC_TRIE_VALUE_TYPE int
#define RC_TRIE_HASH(k)    0
#define RC_TRIE_NAME       rc_trie_collide
#include "richc/template/hash_trie.h"

RC_TEST_GROUP_DATA(hash_trie) {
    rc_arena a;
};

RC_TEST_GROUP_INIT(hash_trie, fix)
{
    fix->a = rc_arena_make_default();
}

RC_TEST_GROUP_DEINIT(hash_trie, fix)
{
    rc_arena_destroy(&fix->a);
}

RC_TEST_STEP(hash_trie, empty, fix)
{
    rc_trie_test_pool pool = rc_trie_test_pool_make(0, &fix->a);
    rc_trie_test t = rc_trie_test_make(&pool, &fix->a);
    RC_CHECK_TRUE(rc_trie_test_find(&t, 123) == NULL);
    RC_CHECK_FALSE(rc_trie_test_delete(&t, 123));
}

RC_TEST_STEP(hash_trie, add_find, fix)
{
    rc_trie_test_pool pool = rc_trie_test_pool_make(0, &fix->a);
    rc_trie_test t = rc_trie_test_make(&pool, &fix->a);

    RC_CHECK_TRUE(rc_trie_test_add(&t, 10, 100, &fix->a));    // new key
    RC_CHECK_TRUE(rc_trie_test_add(&t, 20, 200, &fix->a));
    int *v = rc_trie_test_find(&t, 10);
    RC_CHECK_TRUE(v != NULL);
    RC_CHECK(*v, ==, 100);
    RC_CHECK(*rc_trie_test_find(&t, 20), ==, 200);

    // re-adding an existing key updates it and returns false
    RC_CHECK_FALSE(rc_trie_test_add(&t, 10, 999, &fix->a));
    RC_CHECK(*rc_trie_test_find(&t, 10), ==, 999);

    RC_CHECK_TRUE(rc_trie_test_find(&t, 30) == NULL);
}

RC_TEST_STEP(hash_trie, collisions, fix)
{
    // 0x10, 0x20, 0x30 share the low nibble (collide at level 0) and diverge at
    // level 1, exercising the child-block descent
    rc_trie_test_pool pool = rc_trie_test_pool_make(0, &fix->a);
    rc_trie_test t = rc_trie_test_make(&pool, &fix->a);
    rc_trie_test_add(&t, 0x10, 1, &fix->a);
    rc_trie_test_add(&t, 0x20, 2, &fix->a);
    rc_trie_test_add(&t, 0x30, 3, &fix->a);
    RC_CHECK(*rc_trie_test_find(&t, 0x10), ==, 1);
    RC_CHECK(*rc_trie_test_find(&t, 0x20), ==, 2);
    RC_CHECK(*rc_trie_test_find(&t, 0x30), ==, 3);
}

RC_TEST_STEP(hash_trie, delete_leaf, fix)
{
    rc_trie_test_pool pool = rc_trie_test_pool_make(0, &fix->a);
    rc_trie_test t = rc_trie_test_make(&pool, &fix->a);
    rc_trie_test_add(&t, 10, 100, &fix->a);
    rc_trie_test_add(&t, 20, 200, &fix->a);

    RC_CHECK_TRUE(rc_trie_test_delete(&t, 10));
    RC_CHECK_TRUE(rc_trie_test_find(&t, 10) == NULL);
    RC_CHECK(*rc_trie_test_find(&t, 20), ==, 200);    // sibling intact
    RC_CHECK_FALSE(rc_trie_test_delete(&t, 10));      // already gone
}

RC_TEST_STEP(hash_trie, delete_interior, fix)
{
    // 0x10 owns the level-0 slot and has 0x20/0x30 as children; deleting it
    // forces a bubble-up of a child into the vacated interior node
    rc_trie_test_pool pool = rc_trie_test_pool_make(0, &fix->a);
    rc_trie_test t = rc_trie_test_make(&pool, &fix->a);
    rc_trie_test_add(&t, 0x10, 1, &fix->a);
    rc_trie_test_add(&t, 0x20, 2, &fix->a);
    rc_trie_test_add(&t, 0x30, 3, &fix->a);

    RC_CHECK_TRUE(rc_trie_test_delete(&t, 0x10));
    RC_CHECK_TRUE(rc_trie_test_find(&t, 0x10) == NULL);
    RC_CHECK(*rc_trie_test_find(&t, 0x20), ==, 2);
    RC_CHECK(*rc_trie_test_find(&t, 0x30), ==, 3);
}

RC_TEST_STEP(hash_trie, delete_all, fix)
{
    // deleting both keys empties the child block; the parent link is cleared and
    // re-adding still works
    rc_trie_test_pool pool = rc_trie_test_pool_make(0, &fix->a);
    rc_trie_test t = rc_trie_test_make(&pool, &fix->a);
    rc_trie_test_add(&t, 0x10, 1, &fix->a);
    rc_trie_test_add(&t, 0x20, 2, &fix->a);

    RC_CHECK_TRUE(rc_trie_test_delete(&t, 0x20));
    RC_CHECK_TRUE(rc_trie_test_delete(&t, 0x10));
    RC_CHECK_TRUE(rc_trie_test_find(&t, 0x10) == NULL);
    RC_CHECK_TRUE(rc_trie_test_find(&t, 0x20) == NULL);

    RC_CHECK_TRUE(rc_trie_test_add(&t, 0x10, 5, &fix->a));
    RC_CHECK(*rc_trie_test_find(&t, 0x10), ==, 5);
}

RC_TEST_STEP(hash_trie, many, fix)
{
    // 200 keys force several block allocations (and pool reallocation); every
    // key must survive the moves
    rc_trie_test_pool pool = rc_trie_test_pool_make(0, &fix->a);
    rc_trie_test t = rc_trie_test_make(&pool, &fix->a);

    for (uint64_t k = 1; k <= 200; k++) {
        RC_CHECK_TRUE(rc_trie_test_add(&t, k, (int)(k * 2), &fix->a));
    }
    for (uint64_t k = 1; k <= 200; k++) {
        int *v = rc_trie_test_find(&t, k);
        RC_CHECK_TRUE(v != NULL);
        RC_CHECK(*v, ==, (int)(k * 2));
    }
    // delete the even keys; odd keys remain
    for (uint64_t k = 2; k <= 200; k += 2) {
        RC_CHECK_TRUE(rc_trie_test_delete(&t, k));
    }
    for (uint64_t k = 1; k <= 200; k++) {
        int *v = rc_trie_test_find(&t, k);
        if (k % 2 == 0) {
            RC_CHECK_TRUE(v == NULL);
        }
        else {
            RC_CHECK_TRUE(v != NULL);
            RC_CHECK(*v, ==, (int)(k * 2));
        }
    }
}

RC_TEST_STEP(hash_trie, shared_pool, fix)
{
    // two tries on one pool stay independent
    rc_trie_test_pool pool = rc_trie_test_pool_make(0, &fix->a);
    rc_trie_test a = rc_trie_test_make(&pool, &fix->a);
    rc_trie_test b = rc_trie_test_make(&pool, &fix->a);

    rc_trie_test_add(&a, 42, 1, &fix->a);
    rc_trie_test_add(&b, 42, 2, &fix->a);
    RC_CHECK(*rc_trie_test_find(&a, 42), ==, 1);
    RC_CHECK(*rc_trie_test_find(&b, 42), ==, 2);

    rc_trie_test_add(&a, 99, 9, &fix->a);
    RC_CHECK_TRUE(rc_trie_test_find(&b, 99) == NULL);   // b unaffected
}

RC_TEST_STEP(hash_trie, deep_chain, fix)
{
    // every key hashes to 0, forming a single deep chain
    rc_trie_collide_pool pool = rc_trie_collide_pool_make(0, &fix->a);
    rc_trie_collide t = rc_trie_collide_make(&pool, &fix->a);

    for (uint64_t k = 1; k <= 20; k++) {
        RC_CHECK_TRUE(rc_trie_collide_add(&t, k, (int)k, &fix->a));
    }
    for (uint64_t k = 1; k <= 20; k++) {
        RC_CHECK(*rc_trie_collide_find(&t, k), ==, (int)k);
    }
    // delete from the middle of the chain; the rest survive
    RC_CHECK_TRUE(rc_trie_collide_delete(&t, 10));
    RC_CHECK_TRUE(rc_trie_collide_find(&t, 10) == NULL);
    for (uint64_t k = 1; k <= 20; k++) {
        if (k != 10) {
            RC_CHECK_TRUE(rc_trie_collide_find(&t, k) != NULL);
        }
    }
}

RC_TEST_STEP(hash_trie, pool_lifecycle, fix)
{
    rc_trie_test_pool pool = rc_trie_test_pool_make(4, &fix->a);   // pre-reserve 4 blocks
    RC_CHECK_TRUE(pool.cap >= 4u);
    rc_trie_test_pool_reserve(&pool, 32, &fix->a);
    RC_CHECK_TRUE(pool.cap >= 32u);

    rc_trie_test t = rc_trie_test_make(&pool, &fix->a);
    rc_trie_test_add(&t, 7, 70, &fix->a);
    RC_CHECK(*rc_trie_test_find(&t, 7), ==, 70);

    rc_trie_test_pool_destroy(&pool, &fix->a);
    RC_CHECK(pool.num, ==, 0u);
    RC_CHECK(pool.cap, ==, 0u);
    RC_CHECK_TRUE(pool.data == NULL);
}
