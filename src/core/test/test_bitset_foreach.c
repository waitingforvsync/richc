#include "richc/arena.h"
#include "richc/bitset.h"
#include "richc/test.h"

// context callback: record each visited set-bit index, in order
typedef struct { uint32_t indices[64]; uint32_t count; } collect_ctx;
static void collect(collect_ctx *c, uint32_t i) { c->indices[c->count++] = i; }

#define RC_BITSET_FOREACH_CTX        collect_ctx
#define RC_BITSET_FOREACH_FUNC(c, i) collect(c, i)
#include "richc/template/algorithm/bitset_foreach.h"   // rc_bitset_foreach

// non-context callback accumulating through file-scope state
static uint32_t g_count;
static uint64_t g_mask;
static void note(uint32_t i) { g_count++; g_mask |= (uint64_t)1 << i; }

#define RC_BITSET_FOREACH_FUNC(i) note(i)
#define RC_BITSET_FOREACH_NAME    rc_bitset_foreach_note
#include "richc/template/algorithm/bitset_foreach.h"

RC_TEST_GROUP_DATA(bitset_foreach) {
    rc_arena a;
};

RC_TEST_GROUP_INIT(bitset_foreach, fix)
{
    fix->a = rc_arena_make_default();
}

RC_TEST_GROUP_DEINIT(bitset_foreach, fix)
{
    rc_arena_deinit(&fix->a);
}

RC_TEST(bitset_foreach, empty)
{
    rc_bitset bs = {0};
    collect_ctx ctx = {0};
    rc_bitset_foreach(&bs, &ctx);
    RC_CHECK(ctx.count, ==, 0u);
}

RC_TEST_STEP(bitset_foreach, no_bits_set, fix)
{
    rc_bitset bs = {0};
    rc_bitset_resize(&bs, 100, &fix->a);   // sized but all clear
    collect_ctx ctx = {0};
    rc_bitset_foreach(&bs, &ctx);
    RC_CHECK(ctx.count, ==, 0u);
}

RC_TEST_STEP(bitset_foreach, single, fix)
{
    rc_bitset bs = {0};
    rc_bitset_resize(&bs, 64, &fix->a);
    rc_bitset_set(&bs, 40);
    collect_ctx ctx = {0};
    rc_bitset_foreach(&bs, &ctx);
    RC_CHECK(ctx.count, ==, 1u);
    RC_CHECK(ctx.indices[0], ==, 40u);
}

RC_TEST_STEP(bitset_foreach, across_words, fix)
{
    rc_bitset bs = {0};
    rc_bitset_resize(&bs, 256, &fix->a);
    uint32_t bits[] = {0, 1, 31, 32, 63, 64, 100, 200, 255};
    uint32_t n = (uint32_t)(sizeof(bits) / sizeof(bits[0]));
    for (uint32_t i = 0; i < n; i++) rc_bitset_set(&bs, bits[i]);

    collect_ctx ctx = {0};
    rc_bitset_foreach(&bs, &ctx);
    RC_CHECK(ctx.count, ==, n);
    for (uint32_t i = 0; i < n; i++) RC_CHECK(ctx.indices[i], ==, bits[i]);
}

RC_TEST_STEP(bitset_foreach, non_context, fix)
{
    rc_bitset bs = {0};
    rc_bitset_resize(&bs, 64, &fix->a);
    rc_bitset_set(&bs, 3);
    rc_bitset_set(&bs, 17);
    rc_bitset_set(&bs, 42);

    g_count = 0;
    g_mask = 0;
    rc_bitset_foreach_note(&bs);
    RC_CHECK(g_count, ==, 3u);
    RC_CHECK_TRUE(g_mask == (((uint64_t)1 << 3) | ((uint64_t)1 << 17) | ((uint64_t)1 << 42)));
}
