#include "richc/bytes.h"
#include "richc/test.h"

RC_TEST_GROUP_DATA(bytes) {
    rc_arena a;
};

RC_TEST_GROUP_INIT(bytes, fix)
{
    fix->a = rc_arena_make_default();
}

RC_TEST_GROUP_DEINIT(bytes, fix)
{
    rc_arena_destroy(&fix->a);
}

RC_TEST_STEP(bytes, array, fix)
{
    rc_array_bytes a = rc_array_bytes_make(0, &fix->a);
    rc_array_bytes_push(&a, 0xDE, &fix->a);
    rc_array_bytes_push(&a, 0xAD, &fix->a);
    RC_CHECK(a.num, ==, 2u);
    RC_CHECK(rc_array_bytes_get(&a, 0), ==, (uint8_t)0xDE);
    RC_CHECK(rc_array_bytes_get(&a, 1), ==, (uint8_t)0xAD);
}

RC_TEST(bytes, view_span)
{
    uint8_t raw[] = {1, 2, 3};
    rc_view_bytes v = RC_VIEW(raw);
    RC_CHECK(v.num, ==, 3u);
    RC_CHECK(rc_view_bytes_get(v, 2), ==, (uint8_t)3);

    rc_span_bytes s = RC_SPAN(raw);
    rc_span_bytes_set(s, 0, 0xFF);
    RC_CHECK(raw[0], ==, (uint8_t)0xFF);
}

RC_TEST_STEP(bytes, make_copy, fix)
{
    uint8_t raw[] = {10, 20, 30};
    rc_view_bytes v = RC_VIEW(raw);
    rc_array_bytes a = rc_array_bytes_make_copy(v, 0, &fix->a);
    RC_CHECK(a.num, ==, 3u);
    RC_CHECK(rc_array_bytes_get(&a, 1), ==, (uint8_t)20);
}
