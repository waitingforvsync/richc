#include <stdint.h>

#include "richc/arena.h"
#include "richc/test.h"

// Most tests run against a fresh default arena provided by this fixture.
RC_TEST_GROUP_DATA(arena) {
    rc_arena a;
};

RC_TEST_GROUP_INIT(arena, data)
{
    data->a = rc_arena_make_default();
}

RC_TEST_GROUP_DEINIT(arena, data)
{
    rc_arena_destroy(&data->a);
}

/* ---- lifecycle (manage their own arenas) ---- */

RC_TEST(arena, make_default)
{
    rc_arena a = rc_arena_make_default();
    RC_CHECK_TRUE(a.base != NULL);
    RC_CHECK(a.top, ==, 0u);
    RC_CHECK_TRUE(a.committed > 0u);
    RC_CHECK(a.reserved, ==, RC_ARENA_DEFAULT_RESERVE);
    rc_arena_destroy(&a);
}

RC_TEST(arena, make_rounds_to_page)
{
    // A sub-page request still reserves and commits exactly one page.
    rc_arena a = rc_arena_make(100);
    RC_CHECK_TRUE(a.base != NULL);
    RC_CHECK_TRUE(a.reserved >= 100u);
    RC_CHECK(a.reserved, ==, a.committed);
    RC_CHECK(a.top, ==, 0u);
    rc_arena_destroy(&a);
}

RC_TEST(arena, make_zero)
{
    // A zero reserve rounds to zero and fails, yielding a null arena.
    rc_arena a = rc_arena_make(0);
    RC_CHECK_TRUE(a.base == NULL);
}

RC_TEST(arena, destroy)
{
    rc_arena a = rc_arena_make_default();
    rc_arena_destroy(&a);
    RC_CHECK_TRUE(a.base == NULL);
    RC_CHECK(a.top, ==, 0u);
    RC_CHECK(a.committed, ==, 0u);
    RC_CHECK(a.reserved, ==, 0u);
}

RC_TEST(arena, alloc_fills_reservation)
{
    rc_arena a = rc_arena_make(1);   // one page
    RC_CHECK_TRUE(a.base != NULL);
    // Allocating exactly the whole reservation succeeds (and never returns NULL).
    void *p = rc_arena_alloc(&a, a.reserved);
    RC_CHECK_TRUE(p != NULL);
    rc_arena_destroy(&a);
}

/* ---- alloc ---- */

RC_TEST_STEP(arena, alloc_basic, data)
{
    void *p = rc_arena_alloc(&data->a, 100);
    RC_CHECK_TRUE(p != NULL);
    RC_CHECK_TRUE(p == data->a.base);     // first allocation sits at the base
    RC_CHECK_TRUE(data->a.top >= 100u);   // top advanced past the request
}

RC_TEST_STEP(arena, alloc_alignment, data)
{
    uint8_t *p1 = rc_arena_alloc(&data->a, 1);
    uint8_t *p2 = rc_arena_alloc(&data->a, 1);
    RC_CHECK_TRUE((uintptr_t)p1 % RC_MAX_ALIGN == 0);
    RC_CHECK_TRUE((uintptr_t)p2 % RC_MAX_ALIGN == 0);
    // a 1-byte request is rounded up to a full alignment unit
    RC_CHECK_TRUE((uint32_t)(p2 - p1) == RC_MAX_ALIGN);
}

RC_TEST_STEP(arena, alloc_zero, data)
{
    // Dirty some memory, free it, then alloc_zero the reclaimed space.
    uint8_t *p = rc_arena_alloc(&data->a, 64);
    RC_CHECK_TRUE(p != NULL);
    for (uint32_t i = 0; i < 64; i++) p[i] = 0xFF;
    rc_arena_free(&data->a, p, 64);

    uint8_t *q = rc_arena_alloc_zero(&data->a, 64);
    RC_CHECK_TRUE(q == p);   // same memory reused
    for (uint32_t i = 0; i < 64; i++) RC_CHECK(q[i], ==, (uint8_t)0);
}

RC_TEST_STEP(arena, alloc_type, data)
{
    int *p = rc_arena_alloc_type(&data->a, int, 10);
    RC_CHECK_TRUE(p != NULL);
    for (int i = 0; i < 10; i++) p[i] = i * 3;
    for (int i = 0; i < 10; i++) RC_CHECK(p[i], ==, i * 3);
}

RC_TEST_STEP(arena, alloc_zero_type, data)
{
    // Reuse dirtied memory to prove alloc_zero_type clears it.
    uint8_t *dirty = rc_arena_alloc(&data->a, sizeof(int) * 8);
    for (uint32_t i = 0; i < sizeof(int) * 8; i++) dirty[i] = 0xFF;
    rc_arena_free(&data->a, dirty, sizeof(int) * 8);

    int *p = rc_arena_alloc_zero_type(&data->a, int, 8);
    RC_CHECK_TRUE((uint8_t *)p == dirty);
    for (int i = 0; i < 8; i++) RC_CHECK(p[i], ==, 0);
}

/* ---- free ---- */

RC_TEST_STEP(arena, free_last, data)
{
    uint32_t top0 = data->a.top;
    void *p = rc_arena_alloc(&data->a, 64);
    RC_CHECK_TRUE(p != NULL);
    RC_CHECK_TRUE(rc_arena_free(&data->a, p, 64));
    RC_CHECK(data->a.top, ==, top0);
}

RC_TEST_STEP(arena, free_not_last, data)
{
    void *p1 = rc_arena_alloc(&data->a, 64);
    void *p2 = rc_arena_alloc(&data->a, 64);
    uint32_t top = data->a.top;
    RC_CHECK_FALSE(rc_arena_free(&data->a, p1, 64));  // mid-arena cannot be freed
    RC_CHECK(data->a.top, ==, top);
    (void)p2;
}

RC_TEST_STEP(arena, free_to, data)
{
    uint32_t mark = data->a.top;
    rc_arena_alloc(&data->a, 4096);
    RC_CHECK_TRUE(data->a.top > mark);
    rc_arena_free_to(&data->a, mark);
    RC_CHECK(data->a.top, ==, mark);
}

/* ---- realloc ---- */

RC_TEST_STEP(arena, realloc_as_alloc, data)
{
    // A NULL pointer makes realloc behave like alloc.
    void *p = rc_arena_realloc(&data->a, NULL, 0, 100);
    RC_CHECK_TRUE(p != NULL);
}

RC_TEST_STEP(arena, realloc_grow_in_place, data)
{
    uint8_t *p = rc_arena_alloc(&data->a, 32);
    for (uint32_t i = 0; i < 32; i++) p[i] = (uint8_t)(i + 1);
    uint8_t *q = rc_arena_realloc(&data->a, p, 32, 96);
    RC_CHECK_TRUE(q == p);   // the last allocation grows in place
    for (uint32_t i = 0; i < 32; i++) RC_CHECK(q[i], ==, (uint8_t)(i + 1));
}

RC_TEST_STEP(arena, realloc_shrink_in_place, data)
{
    uint8_t *p = rc_arena_alloc(&data->a, 128);
    uint32_t top_before = data->a.top;
    uint8_t *q = rc_arena_realloc(&data->a, p, 128, 32);
    RC_CHECK_TRUE(q == p);
    RC_CHECK_TRUE(data->a.top < top_before);  // shrinking the last block frees the tail
}

RC_TEST_STEP(arena, realloc_move, data)
{
    uint8_t *p = rc_arena_alloc(&data->a, 16);
    for (uint32_t i = 0; i < 16; i++) p[i] = (uint8_t)(i + 1);
    uint8_t *blocker = rc_arena_alloc(&data->a, 16);   // p is no longer the last block
    uint8_t *q = rc_arena_realloc(&data->a, p, 16, 64);
    RC_CHECK_TRUE(q != p);                             // had to relocate
    for (uint32_t i = 0; i < 16; i++) RC_CHECK(q[i], ==, (uint8_t)(i + 1));  // contents copied
    (void)blocker;
}

RC_TEST_STEP(arena, realloc_shrink_not_last, data)
{
    uint8_t *p = rc_arena_alloc(&data->a, 128);
    uint8_t *blocker = rc_arena_alloc(&data->a, 16);
    uint8_t *q = rc_arena_realloc(&data->a, p, 128, 32);
    RC_CHECK_TRUE(q == p);   // shrinking a mid-arena block is a no-op
    (void)blocker;
}

RC_TEST_STEP(arena, realloc_zero_grows, data)
{
    uint8_t *p = rc_arena_alloc(&data->a, 16);
    for (uint32_t i = 0; i < 16; i++) p[i] = 0xFF;
    uint8_t *q = rc_arena_realloc_zero(&data->a, p, 16, 48);
    RC_CHECK_TRUE(q == p);                                          // grows in place
    for (uint32_t i = 0; i < 16; i++) RC_CHECK(q[i], ==, (uint8_t)0xFF);  // old bytes kept
    for (uint32_t i = 16; i < 48; i++) RC_CHECK(q[i], ==, (uint8_t)0);    // new bytes zeroed
}

/* ---- reset, scratch, stability ---- */

RC_TEST_STEP(arena, reset, data)
{
    void *p = rc_arena_alloc(&data->a, 200000);   // commit several pages
    RC_CHECK_TRUE(p != NULL);
    uint32_t committed_before = data->a.committed;
    rc_arena_reset(&data->a);
    RC_CHECK(data->a.top, ==, 0u);
    RC_CHECK_TRUE(data->a.committed < committed_before);  // pages returned to the OS
    void *q = rc_arena_alloc(&data->a, 10);
    RC_CHECK_TRUE(q == data->a.base);  // allocations restart at the base
}

RC_TEST_STEP(arena, scratch, data)
{
    rc_arena scratch = data->a;        // by-value snapshot
    uint32_t top_before = data->a.top;
    void *p = rc_arena_alloc(&scratch, 1000);
    RC_CHECK_TRUE(p != NULL);
    RC_CHECK_TRUE(scratch.top > top_before);   // the local copy advances
    RC_CHECK(data->a.top, ==, top_before);     // the original is untouched
}

RC_TEST_STEP(arena, pointer_stability, data)
{
    uint8_t *first = rc_arena_alloc(&data->a, 8);
    RC_CHECK_TRUE(first != NULL);
    first[0] = 0xAB;
    // Force many allocations and page commits; the base never moves.
    for (int k = 0; k < 1000; k++) {
        void *p = rc_arena_alloc(&data->a, 1024);
        RC_CHECK_TRUE(p != NULL);
    }
    RC_CHECK(first[0], ==, (uint8_t)0xAB);
}
