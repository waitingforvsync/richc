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
    rc_arena_deinit(&fix->a);
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

RC_TEST(bytes, as_str)
{
    // a byte view/span reinterprets as a read-only rc_str with no copy
    uint8_t raw[] = {'h', 'i'};
    rc_view_bytes v = RC_VIEW(raw);
    rc_str sv = rc_view_bytes_as_str(v);
    RC_CHECK(sv, ==, RC_STR("hi"));
    RC_CHECK_TRUE((const uint8_t *)sv.data == raw);   // same memory, no copy

    rc_span_bytes s = RC_SPAN(raw);
    RC_CHECK(rc_span_bytes_as_str(s), ==, RC_STR("hi"));

    RC_CHECK_FALSE(rc_str_is_valid(rc_view_bytes_as_str(rc_view_bytes_make(NULL, 0))));
}

RC_TEST_STEP(bytes, to_mstr, fix)
{
    // a byte buffer holding "hello"; to_mstr appends the '\0' and reinterprets it
    uint8_t raw[] = {'h', 'e', 'l', 'l', 'o'};
    rc_view_bytes v = RC_VIEW(raw);
    rc_array_bytes a = rc_array_bytes_make_copy(v, 0, &fix->a);   // num 5, cap 5

    rc_mstr s = rc_array_bytes_to_mstr(&a, &fix->a);
    RC_CHECK_TRUE(rc_mstr_is_valid(&s));
    RC_CHECK(s.view, ==, RC_STR("hello"));
    RC_CHECK(s.len, ==, 5u);
    RC_CHECK_TRUE(s.data[5] == '\0');
    RC_CHECK_TRUE(s.len + 1 <= s.cap);            // the rc_mstr invariant holds

    // ownership moved to the mstr: the source array is reset to the empty { 0 }
    RC_CHECK_TRUE(a.data == NULL);
    RC_CHECK(a.num, ==, 0u);
    RC_CHECK(a.cap, ==, 0u);
}
