#include "richc/str.h"
#include "richc/test.h"

/* ---- construction ---- */

RC_TEST(str, literal)
{
    rc_str s = RC_STR("hello");
    RC_CHECK(s.len, ==, 5u);
    RC_CHECK(s, ==, rc_str_make("hello"));

    rc_str empty = RC_STR("");
    RC_CHECK(empty.len, ==, 0u);
    RC_CHECK_TRUE(rc_str_is_valid(empty));
}

RC_TEST(str, make)
{
    rc_str s = rc_str_make("hello");
    RC_CHECK(s.len, ==, 5u);
    RC_CHECK(s, ==, RC_STR("hello"));
}

RC_TEST(str, make_null)
{
    rc_str s = rc_str_make(NULL);
    RC_CHECK_FALSE(rc_str_is_valid(s));
    RC_CHECK_TRUE(rc_str_is_empty(s));
    RC_CHECK(s.len, ==, 0u);
}

RC_TEST(str, make_empty)
{
    rc_str s = rc_str_make("");
    RC_CHECK_TRUE(rc_str_is_valid(s));
    RC_CHECK_TRUE(rc_str_is_empty(s));
}

/* ---- predicates ---- */

RC_TEST(str, is_valid)
{
    RC_CHECK_TRUE(rc_str_is_valid(RC_STR("x")));
    RC_CHECK_TRUE(rc_str_is_valid(RC_STR("")));
    RC_CHECK_FALSE(rc_str_is_valid(rc_str_make(NULL)));
}

RC_TEST(str, is_empty)
{
    RC_CHECK_FALSE(rc_str_is_empty(RC_STR("x")));
    RC_CHECK_TRUE(rc_str_is_empty(RC_STR("")));
    RC_CHECK_TRUE(rc_str_is_empty(rc_str_make(NULL)));
}

RC_TEST(str, is_equal)
{
    RC_CHECK_TRUE(rc_str_is_equal(RC_STR("abc"), RC_STR("abc")));
    RC_CHECK_FALSE(rc_str_is_equal(RC_STR("abc"), RC_STR("abd")));
    RC_CHECK_FALSE(rc_str_is_equal(RC_STR("abc"), RC_STR("ab")));
}

RC_TEST(str, is_equal_empty)
{
    // Any two zero-length views compare equal, including the invalid view.
    RC_CHECK_TRUE(rc_str_is_equal(RC_STR(""), RC_STR("")));
    RC_CHECK_TRUE(rc_str_is_equal(rc_str_make(NULL), RC_STR("")));
}

RC_TEST(str, is_equal_insensitive)
{
    RC_CHECK_TRUE(rc_str_is_equal_insensitive(RC_STR("Hello"), RC_STR("hELLO")));
    RC_CHECK_FALSE(rc_str_is_equal_insensitive(RC_STR("Hello"), RC_STR("World")));
    RC_CHECK_FALSE(rc_str_is_equal_insensitive(RC_STR("abc"), RC_STR("ab")));
}

RC_TEST(str, compare)
{
    RC_CHECK(rc_str_compare(RC_STR("abc"), RC_STR("abc")), ==, 0);
    RC_CHECK(rc_str_compare(RC_STR("abc"), RC_STR("abd")), <, 0);
    RC_CHECK(rc_str_compare(RC_STR("abd"), RC_STR("abc")), >, 0);
}

RC_TEST(str, compare_length)
{
    // When one is a prefix of the other, the shorter sorts first.
    RC_CHECK(rc_str_compare(RC_STR("ab"), RC_STR("abc")), <, 0);
    RC_CHECK(rc_str_compare(RC_STR("abc"), RC_STR("ab")), >, 0);
    RC_CHECK(rc_str_compare(RC_STR(""), RC_STR("a")), <, 0);
}

RC_TEST(str, compare_insensitive)
{
    RC_CHECK(rc_str_compare_insensitive(RC_STR("ABC"), RC_STR("abc")), ==, 0);
    RC_CHECK(rc_str_compare_insensitive(RC_STR("abc"), RC_STR("ABD")), <, 0);
    RC_CHECK(rc_str_compare_insensitive(RC_STR("ab"), RC_STR("ABC")), <, 0);
}

/* ---- slicing ---- */

RC_TEST(str, left)
{
    RC_CHECK(rc_str_left(RC_STR("hello"), 3), ==, RC_STR("hel"));
    RC_CHECK(rc_str_left(RC_STR("hello"), 0), ==, RC_STR(""));
    // count clamps to len
    RC_CHECK(rc_str_left(RC_STR("hello"), 99), ==, RC_STR("hello"));
}

RC_TEST(str, right)
{
    RC_CHECK(rc_str_right(RC_STR("hello"), 2), ==, RC_STR("lo"));
    RC_CHECK(rc_str_right(RC_STR("hello"), 0), ==, RC_STR(""));
    RC_CHECK(rc_str_right(RC_STR("hello"), 99), ==, RC_STR("hello"));
}

RC_TEST(str, substr)
{
    RC_CHECK(rc_str_substr(RC_STR("hello world"), 6, 5), ==, RC_STR("world"));
    // start clamps to len, giving an empty view
    RC_CHECK(rc_str_substr(RC_STR("hello"), 10, 3), ==, RC_STR(""));
    // count clamps to what remains
    RC_CHECK(rc_str_substr(RC_STR("hello"), 3, 99), ==, RC_STR("lo"));
}

RC_TEST(str, skip)
{
    RC_CHECK(rc_str_skip(RC_STR("hello"), 2), ==, RC_STR("llo"));
    RC_CHECK(rc_str_skip(RC_STR("hello"), 0), ==, RC_STR("hello"));
    // start clamps to len, giving an empty view
    RC_CHECK(rc_str_skip(RC_STR("hello"), 99), ==, RC_STR(""));
}

/* ---- searching ---- */

RC_TEST(str, starts_with)
{
    RC_CHECK_TRUE(rc_str_starts_with(RC_STR("hello"), RC_STR("he")));
    RC_CHECK_TRUE(rc_str_starts_with(RC_STR("hello"), RC_STR("hello")));
    RC_CHECK_FALSE(rc_str_starts_with(RC_STR("hello"), RC_STR("lo")));
    // empty prefix always matches; a prefix longer than s never matches
    RC_CHECK_TRUE(rc_str_starts_with(RC_STR("hello"), RC_STR("")));
    RC_CHECK_FALSE(rc_str_starts_with(RC_STR("he"), RC_STR("hello")));
}

RC_TEST(str, ends_with)
{
    RC_CHECK_TRUE(rc_str_ends_with(RC_STR("hello"), RC_STR("lo")));
    RC_CHECK_TRUE(rc_str_ends_with(RC_STR("hello"), RC_STR("hello")));
    RC_CHECK_FALSE(rc_str_ends_with(RC_STR("hello"), RC_STR("he")));
    RC_CHECK_TRUE(rc_str_ends_with(RC_STR("hello"), RC_STR("")));
    RC_CHECK_FALSE(rc_str_ends_with(RC_STR("lo"), RC_STR("hello")));
}

RC_TEST(str, find_first)
{
    RC_CHECK(rc_str_find_first(RC_STR("hello"), RC_STR("he")), ==, 0u);
    RC_CHECK(rc_str_find_first(RC_STR("hello"), RC_STR("llo")), ==, 2u);
    RC_CHECK(rc_str_find_first(RC_STR("hello"), RC_STR("xyz")), ==, RC_INDEX_NONE);
    // empty needle is found at 0; a needle longer than the haystack is not
    RC_CHECK(rc_str_find_first(RC_STR("hello"), RC_STR("")), ==, 0u);
    RC_CHECK(rc_str_find_first(RC_STR("hi"), RC_STR("hello")), ==, RC_INDEX_NONE);
}

RC_TEST(str, find_first_multi)
{
    // returns the first of several matches
    RC_CHECK(rc_str_find_first(RC_STR("abcabc"), RC_STR("bc")), ==, 1u);
}

RC_TEST(str, find_last)
{
    RC_CHECK(rc_str_find_last(RC_STR("abcabc"), RC_STR("bc")), ==, 4u);
    RC_CHECK(rc_str_find_last(RC_STR("hello"), RC_STR("xyz")), ==, RC_INDEX_NONE);
    // empty needle reports the virtual position past the last character
    RC_CHECK(rc_str_find_last(RC_STR("hello"), RC_STR("")), ==, 5u);
    RC_CHECK(rc_str_find_last(RC_STR("hi"), RC_STR("hello")), ==, RC_INDEX_NONE);
}

RC_TEST(str, contains)
{
    RC_CHECK_TRUE(rc_str_contains(RC_STR("hello"), RC_STR("ell")));
    RC_CHECK_FALSE(rc_str_contains(RC_STR("hello"), RC_STR("xyz")));
    RC_CHECK_TRUE(rc_str_contains(RC_STR("hello"), RC_STR("")));
}

/* ---- trimming ---- */

RC_TEST(str, remove_prefix)
{
    RC_CHECK(rc_str_remove_prefix(RC_STR("foobar"), RC_STR("foo")), ==, RC_STR("bar"));
    // unchanged when the prefix is absent
    RC_CHECK(rc_str_remove_prefix(RC_STR("foobar"), RC_STR("baz")), ==, RC_STR("foobar"));
    RC_CHECK(rc_str_remove_prefix(RC_STR("foobar"), RC_STR("")), ==, RC_STR("foobar"));
}

RC_TEST(str, remove_suffix)
{
    RC_CHECK(rc_str_remove_suffix(RC_STR("foobar"), RC_STR("bar")), ==, RC_STR("foo"));
    RC_CHECK(rc_str_remove_suffix(RC_STR("foobar"), RC_STR("baz")), ==, RC_STR("foobar"));
    RC_CHECK(rc_str_remove_suffix(RC_STR("foobar"), RC_STR("")), ==, RC_STR("foobar"));
}

/* ---- splitting ---- */

RC_TEST(str, first_split)
{
    rc_str_pair p = rc_str_first_split(RC_STR("a/b/c"), RC_STR("/"));
    RC_CHECK(p.first, ==, RC_STR("a"));
    RC_CHECK(p.second, ==, RC_STR("b/c"));
}

RC_TEST(str, first_split_absent)
{
    // with no delimiter, first is the whole string and second is invalid
    rc_str_pair p = rc_str_first_split(RC_STR("abc"), RC_STR("/"));
    RC_CHECK(p.first, ==, RC_STR("abc"));
    RC_CHECK_FALSE(rc_str_is_valid(p.second));
}

RC_TEST(str, last_split)
{
    rc_str_pair p = rc_str_last_split(RC_STR("a/b/c"), RC_STR("/"));
    RC_CHECK(p.first, ==, RC_STR("a/b"));
    RC_CHECK(p.second, ==, RC_STR("c"));
}

RC_TEST(str, last_split_absent)
{
    rc_str_pair p = rc_str_last_split(RC_STR("abc"), RC_STR("/"));
    RC_CHECK(p.first, ==, RC_STR("abc"));
    RC_CHECK_FALSE(rc_str_is_valid(p.second));
}

/* ---- conversion ---- */

RC_TEST(str, as_cstr_fast)
{
    // a view already followed by a null byte is returned without copying
    rc_str s = RC_STR("hello");
    char buf[16];
    const char *c = rc_str_as_cstr(s, buf, sizeof buf);
    RC_CHECK_TRUE(c == s.data);
    RC_CHECK(rc_str_make(c), ==, RC_STR("hello"));
}

RC_TEST(str, as_cstr_copy)
{
    // a view not terminated at [len] is copied into the caller's buffer
    rc_str s = rc_str_left(RC_STR("hello world"), 5);
    char buf[16];
    const char *c = rc_str_as_cstr(s, buf, sizeof buf);
    RC_CHECK_TRUE(c == buf);
    RC_CHECK(rc_str_make(c), ==, RC_STR("hello"));
}

RC_TEST(str, as_cstr_truncate)
{
    // the copy is truncated to fit buf_size, always null-terminated
    rc_str s = rc_str_left(RC_STR("hello world"), 5);
    char buf[3];
    const char *c = rc_str_as_cstr(s, buf, sizeof buf);
    RC_CHECK(rc_str_make(c), ==, RC_STR("he"));
}

RC_TEST(str, as_cstr_null)
{
    // no fast path and no buffer to copy into -> NULL
    rc_str s = rc_str_left(RC_STR("hello world"), 5);
    const char *c = rc_str_as_cstr(s, NULL, 0);
    RC_CHECK_TRUE(c == NULL);
}

RC_TEST_MAIN()
