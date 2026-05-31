#include "richc/arena.h"
#include "richc/mstr.h"
#include "richc/test.h"

// Most tests run against a fresh default arena provided by this fixture.
RC_TEST_GROUP_DATA(mstr) {
    rc_arena a;
};

RC_TEST_GROUP_INIT(mstr, fix)
{
    fix->a = rc_arena_make_default();
}

RC_TEST_GROUP_DEINIT(mstr, fix)
{
    rc_arena_deinit(&fix->a);
}

/* ---- construction ---- */

RC_TEST_STEP(mstr, make, fix)
{
    rc_mstr s = rc_mstr_make(16, &fix->a);
    RC_CHECK_TRUE(rc_mstr_is_valid(&s));
    RC_CHECK_TRUE(rc_mstr_is_empty(&s));
    RC_CHECK(s.len, ==, 0u);
    RC_CHECK(s.cap, ==, 16u);
    RC_CHECK(s.view, ==, RC_STR(""));
    RC_CHECK_TRUE(s.data[0] == '\0');
}

RC_TEST_STEP(mstr, make_zero_cap, fix)
{
    rc_mstr s = rc_mstr_make(0, &fix->a);
    RC_CHECK_TRUE(rc_mstr_is_valid(&s));
    RC_CHECK(s.cap, ==, 0u);
    RC_CHECK_TRUE(s.data[0] == '\0');
}

RC_TEST_STEP(mstr, from_cstr, fix)
{
    rc_mstr s = rc_mstr_from_cstr("hello", 0, &fix->a);
    RC_CHECK(s.view, ==, RC_STR("hello"));
    RC_CHECK(s.len, ==, 5u);
    RC_CHECK(s.cap, ==, 5u);          // max(5, 0)
    RC_CHECK_TRUE(s.data[5] == '\0');
}

RC_TEST_STEP(mstr, from_cstr_max_cap, fix)
{
    rc_mstr s = rc_mstr_from_cstr("hi", 10, &fix->a);
    RC_CHECK(s.view, ==, RC_STR("hi"));
    RC_CHECK(s.len, ==, 2u);
    RC_CHECK(s.cap, ==, 10u);         // max(2, 10)
}

RC_TEST_STEP(mstr, from_cstr_null, fix)
{
    rc_mstr s = rc_mstr_from_cstr(NULL, 4, &fix->a);
    RC_CHECK_FALSE(rc_mstr_is_valid(&s));
    RC_CHECK(s.cap, ==, 0u);
}

RC_TEST_STEP(mstr, from_str, fix)
{
    rc_mstr s = rc_mstr_from_str(RC_STR("world"), 2, &fix->a);
    RC_CHECK(s.view, ==, RC_STR("world"));
    RC_CHECK(s.len, ==, 5u);
    RC_CHECK(s.cap, ==, 5u);          // max(5, 2)
    RC_CHECK_TRUE(s.data[5] == '\0');
}

RC_TEST_STEP(mstr, from_str_invalid, fix)
{
    // The from_* constructors tolerate an invalid source and return invalid.
    rc_mstr s = rc_mstr_from_str(rc_str_make(NULL), 4, &fix->a);
    RC_CHECK_FALSE(rc_mstr_is_valid(&s));
}

/* ---- predicates ---- */

RC_TEST_STEP(mstr, is_empty, fix)
{
    rc_mstr s = rc_mstr_make(8, &fix->a);
    RC_CHECK_TRUE(rc_mstr_is_empty(&s));
    rc_mstr_append_char(&s, 'x', &fix->a);
    RC_CHECK_FALSE(rc_mstr_is_empty(&s));
}

/* ---- mutation ---- */

RC_TEST_STEP(mstr, reset, fix)
{
    rc_mstr s = rc_mstr_from_cstr("hello", 0, &fix->a);
    uint32_t cap = s.cap;
    rc_mstr_reset(&s);
    RC_CHECK(s.len, ==, 0u);
    RC_CHECK_TRUE(rc_mstr_is_empty(&s));
    RC_CHECK(s.cap, ==, cap);         // buffer retained
    RC_CHECK(s.view, ==, RC_STR(""));
    RC_CHECK_TRUE(s.data[0] == '\0');
}

RC_TEST_STEP(mstr, reserve_grows, fix)
{
    rc_mstr s = rc_mstr_from_cstr("abc", 0, &fix->a);
    rc_mstr_reserve(&s, 100, &fix->a);
    RC_CHECK(s.cap, ==, 100u);
    RC_CHECK(s.len, ==, 3u);
    RC_CHECK(s.view, ==, RC_STR("abc"));   // contents preserved across the realloc
}

RC_TEST_STEP(mstr, reserve_noop, fix)
{
    rc_mstr s = rc_mstr_make(50, &fix->a);
    const char *before = s.data;
    rc_mstr_reserve(&s, 10, &fix->a);      // smaller: no-op
    RC_CHECK(s.cap, ==, 50u);
    RC_CHECK_TRUE(s.data == before);       // no reallocation
}

RC_TEST_STEP(mstr, append, fix)
{
    rc_mstr s = rc_mstr_make(0, &fix->a);
    rc_mstr_append(&s, RC_STR("foo"), &fix->a);
    rc_mstr_append(&s, RC_STR("bar"), &fix->a);
    RC_CHECK(s.view, ==, RC_STR("foobar"));
    RC_CHECK(s.len, ==, 6u);
    RC_CHECK_TRUE(s.data[6] == '\0');
}

RC_TEST_STEP(mstr, append_empty, fix)
{
    rc_mstr s = rc_mstr_from_cstr("x", 0, &fix->a);
    rc_mstr_append(&s, RC_STR(""), &fix->a);   // no-op
    RC_CHECK(s.view, ==, RC_STR("x"));
    RC_CHECK(s.len, ==, 1u);
}

RC_TEST_STEP(mstr, append_growth, fix)
{
    rc_mstr s = rc_mstr_make(0, &fix->a);
    for (int i = 0; i < 100; i++)
        rc_mstr_append(&s, RC_STR("ab"), &fix->a);
    RC_CHECK(s.len, ==, 200u);
    RC_CHECK_TRUE(s.cap >= 200u);
    RC_CHECK_TRUE(s.data[200] == '\0');
    RC_CHECK(rc_str_left(s.view, 4), ==, RC_STR("abab"));
}

RC_TEST_STEP(mstr, append_char, fix)
{
    rc_mstr s = rc_mstr_make(0, &fix->a);
    rc_mstr_append_char(&s, 'h', &fix->a);
    rc_mstr_append_char(&s, 'i', &fix->a);
    RC_CHECK(s.view, ==, RC_STR("hi"));
    RC_CHECK(s.len, ==, 2u);
    RC_CHECK_TRUE(s.data[2] == '\0');
}

RC_TEST_STEP(mstr, append_char_growth, fix)
{
    rc_mstr s = rc_mstr_make(0, &fix->a);
    for (int i = 0; i < 50; i++)
        rc_mstr_append_char(&s, 'z', &fix->a);
    RC_CHECK(s.len, ==, 50u);
    RC_CHECK_TRUE(s.cap >= 50u);
    RC_CHECK_TRUE(s.data[50] == '\0');
}

/* ---- replace ---- */

RC_TEST_STEP(mstr, replace_delete, fix)
{
    // replacement shorter than find (here empty): left-to-right rewrite.
    rc_mstr s = rc_mstr_from_cstr("aXbXc", 0, &fix->a);
    rc_mstr_replace(&s, RC_STR("X"), RC_STR(""), &fix->a);
    RC_CHECK(s.view, ==, RC_STR("abc"));
    RC_CHECK(s.len, ==, 3u);
    RC_CHECK_TRUE(s.data[3] == '\0');
}

RC_TEST_STEP(mstr, replace_same_length, fix)
{
    rc_mstr s = rc_mstr_from_cstr("a.b.c", 0, &fix->a);
    rc_mstr_replace(&s, RC_STR("."), RC_STR("-"), &fix->a);
    RC_CHECK(s.view, ==, RC_STR("a-b-c"));
}

RC_TEST_STEP(mstr, replace_longer, fix)
{
    // replacement longer than find: right-to-left rewrite, may reallocate.
    rc_mstr s = rc_mstr_from_cstr("a.b", 0, &fix->a);
    rc_mstr_replace(&s, RC_STR("."), RC_STR("___"), &fix->a);
    RC_CHECK(s.view, ==, RC_STR("a___b"));
    RC_CHECK(s.len, ==, 5u);
    RC_CHECK_TRUE(s.data[5] == '\0');
}

RC_TEST_STEP(mstr, replace_grows_buffer, fix)
{
    // Result (8) exceeds the initial capacity (4), forcing a reserve/realloc.
    rc_mstr s = rc_mstr_from_cstr("xxxx", 0, &fix->a);
    rc_mstr_replace(&s, RC_STR("x"), RC_STR("yy"), &fix->a);
    RC_CHECK(s.view, ==, RC_STR("yyyyyyyy"));
    RC_CHECK(s.len, ==, 8u);
    RC_CHECK_TRUE(s.cap >= 8u);
}

RC_TEST_STEP(mstr, replace_boundaries, fix)
{
    rc_mstr s = rc_mstr_from_cstr("XaX", 0, &fix->a);
    rc_mstr_replace(&s, RC_STR("X"), RC_STR("YY"), &fix->a);
    RC_CHECK(s.view, ==, RC_STR("YYaYY"));
}

RC_TEST_STEP(mstr, replace_not_found, fix)
{
    rc_mstr s = rc_mstr_from_cstr("hello", 0, &fix->a);
    rc_mstr_replace(&s, RC_STR("z"), RC_STR("Q"), &fix->a);   // no-op
    RC_CHECK(s.view, ==, RC_STR("hello"));
    RC_CHECK(s.len, ==, 5u);
}

RC_TEST_STEP(mstr, replace_empty_find, fix)
{
    rc_mstr s = rc_mstr_from_cstr("hello", 0, &fix->a);
    rc_mstr_replace(&s, RC_STR(""), RC_STR("Q"), &fix->a);    // empty find: no-op
    RC_CHECK(s.view, ==, RC_STR("hello"));
}
