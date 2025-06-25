#include "richc/test/test.h"
#include "richc/arena.h"


DEF_TEST(arena, basic) {
    // Make new arena and check that the initial state is as expected
    arena_t arena = arena_make_with_size(4096);
    TEST_REQUIRE_TRUE(arena.base);
    TEST_REQUIRE((uintptr_t)arena.base & 0xFFF, ==, 0);
    TEST_REQUIRE(arena.offset, ==, 0);
    TEST_REQUIRE(arena.size, >=, 4096);

    // Make a first allocation
    char *p1 = arena_alloc(&arena, 1024);
    TEST_REQUIRE_TRUE(p1);
    TEST_REQUIRE(p1 - (char *)arena.base, ==, 0);
    TEST_REQUIRE(arena.offset, ==, 1024);

    // Make a second allocation
    char *p2 = arena_alloc(&arena, 512);
    TEST_REQUIRE_TRUE(p2);
    TEST_REQUIRE(p2 - (char *)arena.base, ==, 1024);
    TEST_REQUIRE(arena.offset, ==, 512+1024);

    // Try to free the first allocation - this should do nothing
    arena_free(&arena, p1, 1024);
    TEST_REQUIRE(arena.offset, ==, 512+1024);

    // Try to free the second allocation - this will move the offset back
    arena_free(&arena, p2, 512);
    TEST_REQUIRE(arena.offset, ==, 1024);

    arena_reset(&arena);
    TEST_REQUIRE(arena.offset, ==, 0);

    arena_deinit(&arena);
    TEST_REQUIRE_TRUE(arena.base);
    TEST_REQUIRE(arena.offset, ==, 0);
    TEST_REQUIRE(arena.size, ==, 0);
}

DEF_TEST(arena, grow) {
    arena_t arena = arena_make_with_size(65536);
    char *original_base = arena.base;

    char *p1 = arena_alloc(&arena, 40000);

    // This allocation won't fit, so the arena needs to be grown: check that the base address remains the same
    char *p2 = arena_alloc(&arena, 30000);
    TEST_REQUIRE_TRUE(arena.base == (void *)original_base);

    char *p3 = arena_realloc(&arena, p2, 30000, 40000);
    TEST_REQUIRE_TRUE(p2 == p3);

    arena_deinit(&arena);
}
