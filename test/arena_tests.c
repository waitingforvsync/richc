#include "richc/test/test.h"
#include "richc/arena.h"


DEF_TEST(arena, basic) {
    // Make new arena and check that the initial state is as expected
    arena_t arena = arena_make_with_size(4096);
    TEST_REQUIRE_TRUE(arena.base);
    TEST_REQUIRE(arena.offset, ==, 32);

    // Make a first allocation
    char *p1 = arena_alloc(&arena, 1024);
    TEST_REQUIRE_TRUE(p1);
    TEST_REQUIRE(p1 - (char *)arena.base, ==, 32);
    TEST_REQUIRE(arena.offset, ==, 1024+32);

    // Make a second allocation
    char *p2 = arena_alloc(&arena, 512);
    TEST_REQUIRE_TRUE(p2);
    TEST_REQUIRE(p2 - (char *)arena.base, ==, 1024+32);
    TEST_REQUIRE(arena.offset, ==, 512+1024+32);

    // Try to free the first allocation - this should do nothing
    arena_free(&arena, p1, 1024);
    TEST_REQUIRE(arena.offset, ==, 512+1024+32);

    // Try to free the second allocation - this will move the offset back
    arena_free(&arena, p2, 512);
    TEST_REQUIRE(arena.offset, ==, 1024+32);

    arena_reset(&arena);
    TEST_REQUIRE(arena.offset, ==, 32);

    arena_deinit(&arena);
    TEST_REQUIRE_FALSE(arena.base);
    TEST_REQUIRE(arena.offset, ==, 0);
}

DEF_TEST(arena, multiple_blocks) {
    // Make new arena and check that the initial state is as expected
    arena_t arena = arena_make_with_size(4096);
    TEST_REQUIRE_TRUE(arena.base);
    TEST_REQUIRE(arena.offset, ==, 32);

}
