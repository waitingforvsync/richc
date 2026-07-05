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

// set trie: no RC_TRIE_VALUE_TYPE, so nodes carry only keys (no value)
#define RC_TRIE_KEY_TYPE   uint64_t
#define RC_TRIE_HASH(k)    (k)
#define RC_TRIE_NAME       rc_trie_set
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
    rc_arena_deinit(&fix->a);
}

RC_TEST_STEP(hash_trie, empty, fix)
{
    rc_trie_test_pool pool = rc_trie_test_pool_make(0, &fix->a);
    rc_trie_test t = rc_trie_test_make(&pool, &fix->a);
    RC_CHECK(rc_trie_test_find(&t, 123), ==, RC_INDEX_NONE);
    RC_CHECK_FALSE(rc_trie_test_delete(&t, 123));
}

RC_TEST_STEP(hash_trie, add_find, fix)
{
    rc_trie_test_pool pool = rc_trie_test_pool_make(0, &fix->a);
    rc_trie_test t = rc_trie_test_make(&pool, &fix->a);

    RC_CHECK_TRUE(rc_trie_test_add(&t, 10, 100, &fix->a));    // new key
    RC_CHECK_TRUE(rc_trie_test_add(&t, 20, 200, &fix->a));

    // find returns a node index; read the value through it
    uint32_t i = rc_trie_test_find(&t, 10);
    RC_CHECK(i, !=, RC_INDEX_NONE);
    RC_CHECK(rc_trie_test_value_get(&t, i), ==, 100);
    // find_ptr returns the value pointer directly
    RC_CHECK(*rc_trie_test_find_ptr(&t, 20), ==, 200);

    // re-adding an existing key updates it and returns false
    RC_CHECK_FALSE(rc_trie_test_add(&t, 10, 999, &fix->a));
    RC_CHECK(rc_trie_test_value_get(&t, rc_trie_test_find(&t, 10)), ==, 999);

    RC_CHECK(rc_trie_test_find(&t, 30), ==, RC_INDEX_NONE);
}

RC_TEST_STEP(hash_trie, value_access, fix)
{
    rc_trie_test_pool pool = rc_trie_test_pool_make(0, &fix->a);
    rc_trie_test t = rc_trie_test_make(&pool, &fix->a);
    rc_trie_test_add(&t, 5, 50, &fix->a);

    uint32_t i = rc_trie_test_find(&t, 5);
    RC_CHECK(i, !=, RC_INDEX_NONE);
    RC_CHECK(rc_trie_test_value_get(&t, i), ==, 50);

    // value_set / value_at mutate the stored value in place
    rc_trie_test_value_set(&t, i, 51);
    RC_CHECK(rc_trie_test_value_get(&t, i), ==, 51);
    *rc_trie_test_value_at(&t, i) = 52;
    RC_CHECK(*rc_trie_test_find_ptr(&t, 5), ==, 52);

    // adding a colliding key forces a child-block allocation (pool growth); the
    // node index from before stays valid (it is logical, not a pointer)
    rc_trie_test_add(&t, 0x15, 150, &fix->a);
    RC_CHECK(rc_trie_test_value_get(&t, i), ==, 52);
    RC_CHECK(*rc_trie_test_find_ptr(&t, 0x15), ==, 150);
}

RC_TEST_STEP(hash_trie, key_access, fix)
{
    rc_trie_test_pool pool = rc_trie_test_pool_make(0, &fix->a);
    rc_trie_test t = rc_trie_test_make(&pool, &fix->a);
    rc_trie_test_add(&t, 10, 100, &fix->a);
    rc_trie_test_add(&t, 0x15, 150, &fix->a);   // collides at level 0, forcing a child block

    // read the key back from a node index, by value and by pointer
    uint32_t i = rc_trie_test_find(&t, 10);
    RC_CHECK(i, !=, RC_INDEX_NONE);
    RC_CHECK(rc_trie_test_key_get(&t, i), ==, (uint64_t)10);
    RC_CHECK(*rc_trie_test_key_at(&t, i), ==, (uint64_t)10);

    // a key that lives one level down (in the child block) reads back the same way
    uint32_t j = rc_trie_test_find(&t, 0x15);
    RC_CHECK(j, !=, RC_INDEX_NONE);
    RC_CHECK(rc_trie_test_key_get(&t, j), ==, (uint64_t)0x15);
}

RC_TEST_STEP(hash_trie, contains, fix)
{
    // contains works on a map trie too (no value needed)
    rc_trie_test_pool pool = rc_trie_test_pool_make(0, &fix->a);
    rc_trie_test t = rc_trie_test_make(&pool, &fix->a);
    rc_trie_test_add(&t, 10, 100, &fix->a);
    RC_CHECK_TRUE(rc_trie_test_contains(&t, 10));
    RC_CHECK_FALSE(rc_trie_test_contains(&t, 20));
    rc_trie_test_delete(&t, 10);
    RC_CHECK_FALSE(rc_trie_test_contains(&t, 10));
}

RC_TEST_STEP(hash_trie, set, fix)
{
    // a set trie: add takes no value; only contains/delete read membership
    rc_trie_set_pool pool = rc_trie_set_pool_make(0, &fix->a);
    rc_trie_set s = rc_trie_set_make(&pool, &fix->a);

    RC_CHECK_FALSE(rc_trie_set_contains(&s, 0x10));
    RC_CHECK_TRUE(rc_trie_set_add(&s, 0x10, &fix->a));     // new
    RC_CHECK_TRUE(rc_trie_set_add(&s, 0x20, &fix->a));
    RC_CHECK_TRUE(rc_trie_set_add(&s, 0x30, &fix->a));     // collide and chain
    RC_CHECK_FALSE(rc_trie_set_add(&s, 0x10, &fix->a));    // already present

    RC_CHECK_TRUE(rc_trie_set_contains(&s, 0x10));
    RC_CHECK_TRUE(rc_trie_set_contains(&s, 0x20));
    RC_CHECK_TRUE(rc_trie_set_contains(&s, 0x30));
    RC_CHECK_FALSE(rc_trie_set_contains(&s, 0x40));

    RC_CHECK_TRUE(rc_trie_set_delete(&s, 0x20));
    RC_CHECK_FALSE(rc_trie_set_contains(&s, 0x20));
    RC_CHECK_TRUE(rc_trie_set_contains(&s, 0x10));         // siblings intact
    RC_CHECK_TRUE(rc_trie_set_contains(&s, 0x30));
    RC_CHECK_FALSE(rc_trie_set_delete(&s, 0x20));          // already gone
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
    RC_CHECK(*rc_trie_test_find_ptr(&t, 0x10), ==, 1);
    RC_CHECK(*rc_trie_test_find_ptr(&t, 0x20), ==, 2);
    RC_CHECK(*rc_trie_test_find_ptr(&t, 0x30), ==, 3);
}

RC_TEST_STEP(hash_trie, delete_leaf, fix)
{
    rc_trie_test_pool pool = rc_trie_test_pool_make(0, &fix->a);
    rc_trie_test t = rc_trie_test_make(&pool, &fix->a);
    rc_trie_test_add(&t, 10, 100, &fix->a);
    rc_trie_test_add(&t, 20, 200, &fix->a);

    RC_CHECK_TRUE(rc_trie_test_delete(&t, 10));
    RC_CHECK(rc_trie_test_find(&t, 10), ==, RC_INDEX_NONE);
    RC_CHECK(*rc_trie_test_find_ptr(&t, 20), ==, 200);    // sibling intact
    RC_CHECK_FALSE(rc_trie_test_delete(&t, 10));          // already gone
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
    RC_CHECK(rc_trie_test_find(&t, 0x10), ==, RC_INDEX_NONE);
    RC_CHECK(*rc_trie_test_find_ptr(&t, 0x20), ==, 2);
    RC_CHECK(*rc_trie_test_find_ptr(&t, 0x30), ==, 3);
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
    RC_CHECK(rc_trie_test_find(&t, 0x10), ==, RC_INDEX_NONE);
    RC_CHECK(rc_trie_test_find(&t, 0x20), ==, RC_INDEX_NONE);

    RC_CHECK_TRUE(rc_trie_test_add(&t, 0x10, 5, &fix->a));
    RC_CHECK(*rc_trie_test_find_ptr(&t, 0x10), ==, 5);
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
        int *v = rc_trie_test_find_ptr(&t, k);
        RC_CHECK_TRUE(v != NULL);
        RC_CHECK(*v, ==, (int)(k * 2));
    }
    // delete the even keys; odd keys remain
    for (uint64_t k = 2; k <= 200; k += 2) {
        RC_CHECK_TRUE(rc_trie_test_delete(&t, k));
    }
    for (uint64_t k = 1; k <= 200; k++) {
        int *v = rc_trie_test_find_ptr(&t, k);
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
    RC_CHECK(*rc_trie_test_find_ptr(&a, 42), ==, 1);
    RC_CHECK(*rc_trie_test_find_ptr(&b, 42), ==, 2);

    rc_trie_test_add(&a, 99, 9, &fix->a);
    RC_CHECK(rc_trie_test_find(&b, 99), ==, RC_INDEX_NONE);   // b unaffected
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
        RC_CHECK(*rc_trie_collide_find_ptr(&t, k), ==, (int)k);
    }
    // delete from the middle of the chain; the rest survive
    RC_CHECK_TRUE(rc_trie_collide_delete(&t, 10));
    RC_CHECK(rc_trie_collide_find(&t, 10), ==, RC_INDEX_NONE);
    for (uint64_t k = 1; k <= 20; k++) {
        if (k != 10) {
            RC_CHECK(rc_trie_collide_find(&t, k), !=, RC_INDEX_NONE);
        }
    }
}

RC_TEST_STEP(hash_trie, pool_lifecycle, fix)
{
    rc_trie_test_pool pool = rc_trie_test_pool_make(4, &fix->a);   // pre-reserve 4 blocks
    RC_CHECK_TRUE(pool.items.cap >= 4u);
    rc_trie_test_pool_reserve(&pool, 32, &fix->a);
    RC_CHECK_TRUE(pool.items.cap >= 32u);

    rc_trie_test t = rc_trie_test_make(&pool, &fix->a);
    rc_trie_test_add(&t, 7, 70, &fix->a);
    RC_CHECK(*rc_trie_test_find_ptr(&t, 7), ==, 70);

    rc_trie_test_pool_deinit(&pool, &fix->a);
    RC_CHECK(pool.items.num, ==, 0u);
    RC_CHECK(pool.items.cap, ==, 0u);
    RC_CHECK_TRUE(pool.items.data == NULL);
    RC_CHECK(pool.first_free, ==, 0u);
}

RC_TEST_STEP(hash_trie, reclaim, fix)
{
    // every key collides into one deep chain, so N keys allocate ~N blocks.
    // adding then deleting them all should recycle the blocks rather than leak,
    // so the block count stays bounded across repeated churn.
    rc_trie_collide_pool pool = rc_trie_collide_pool_make(0, &fix->a);
    rc_trie_collide t = rc_trie_collide_make(&pool, &fix->a);

    for (uint64_t k = 1; k <= 30; k++) {
        rc_trie_collide_add(&t, k, (int)k, &fix->a);
    }
    uint32_t after_first = pool.items.num;   // blocks for one full chain

    for (int cycle = 0; cycle < 5; cycle++) {
        for (uint64_t k = 1; k <= 30; k++) {
            RC_CHECK_TRUE(rc_trie_collide_delete(&t, k));
        }
        for (uint64_t k = 1; k <= 30; k++) {
            RC_CHECK_TRUE(rc_trie_collide_add(&t, k, (int)k, &fix->a));
        }
        // recycled, not leaked: the block count never exceeds the first chain's
        RC_CHECK_TRUE(pool.items.num <= after_first);
        RC_CHECK(*rc_trie_collide_find_ptr(&t, 15), ==, 15);
    }
}
