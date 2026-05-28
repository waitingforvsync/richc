# richc reference

Detailed reference for every public type, macro, and function in richc,
organised by header. As modules are ported into the core layer, their
documentation is added here.

General conventions used throughout the library:

- All public types and functions are prefixed `rc_`; all public macros `RC_`.
- The index type is `uint32_t`. `RC_INDEX_NONE` (`UINT32_MAX`) is the sentinel
  for "not found" / "invalid".
- Functions that allocate take an `rc_arena *` as their last parameter (none of
  the headers documented so far allocate).

## Contents

- [richc/str.h - string view](#richcstrh---string-view)
- [richc/test.h - unit-test framework](#richctesth---unit-test-framework)

---

## richc/str.h - string view

`rc_str` is a non-owning view over character data: a pointer and a length. It
does not own its data and never allocates.

### Types

```c
typedef struct { const char *data; uint32_t len; } rc_str;
typedef struct { rc_str first; rc_str second; } rc_str_pair;
```

A view is in one of three states:

- **Invalid**: `{ NULL, 0 }` - the "no string" / "not found" sentinel.
- **Empty**: `{ ptr, 0 }` - valid but zero length (`ptr` is non-NULL).
- **Valid**: `data` non-NULL, `len` greater than zero.

`rc_str_pair` is returned by the split functions.

### Construction

```c
#define RC_STR(literal)        // compile-time view from a string literal
rc_str rc_str_make(const char *s);
```

- `RC_STR(literal)` builds a view at compile time. Pass only string literals or
  `char[]` arrays; do not pass a `char *` pointer, since the length is computed
  from `sizeof`.
- `rc_str_make(s)` builds a view over a null-terminated C string. Returns the
  invalid view `{ NULL, 0 }` when `s` is NULL.

### Predicates

```c
bool rc_str_is_valid(rc_str s);   // inline; true when data is non-NULL
bool rc_str_is_empty(rc_str s);   // inline; true when len is 0 (also true if invalid)
```

### Comparison

```c
bool rc_str_is_equal(rc_str a, rc_str b);
bool rc_str_is_equal_insensitive(rc_str a, rc_str b);
int  rc_str_compare(rc_str a, rc_str b);
int  rc_str_compare_insensitive(rc_str a, rc_str b);
```

- `rc_str_is_equal` returns true when both have the same length and bytes. Any
  two zero-length views are equal, including the invalid view.
- `rc_str_compare` returns a negative value, zero, or a positive value when `a`
  sorts before, equal to, or after `b`. Comparison is byte-wise; if one view is
  a prefix of the other, the shorter sorts first.
- The `_insensitive` variants fold ASCII case before comparing.

### Slicing

All slicing functions are inline and clamp out-of-range arguments to the valid
range rather than asserting.

```c
rc_str rc_str_left(rc_str s, uint32_t count);                 // first count chars
rc_str rc_str_right(rc_str s, uint32_t count);                // last count chars
rc_str rc_str_substr(rc_str s, uint32_t start, uint32_t count); // count chars from start
rc_str rc_str_skip(rc_str s, uint32_t start);                 // suffix beginning at start
```

`rc_str_left(s, count)` and `rc_str_right(s, count)` clamp `count` to `s.len`.
`rc_str_substr` clamps both `start` and `count`. `rc_str_skip` clamps `start`.

### Searching

```c
bool     rc_str_starts_with(rc_str s, rc_str prefix);
bool     rc_str_ends_with(rc_str s, rc_str suffix);
uint32_t rc_str_find_first(rc_str haystack, rc_str needle);
uint32_t rc_str_find_last(rc_str haystack, rc_str needle);
bool     rc_str_contains(rc_str haystack, rc_str needle);
```

- `rc_str_starts_with` / `rc_str_ends_with` return true for an empty prefix or
  suffix, and false when it is longer than `s`.
- `rc_str_find_first` / `rc_str_find_last` return the index of the match, or
  `RC_INDEX_NONE` when absent or when the needle is longer than the haystack.
  An empty needle is always found: `find_first` returns 0, `find_last` returns
  `haystack.len` (the virtual position just past the last character).
- `rc_str_contains` is true when `rc_str_find_first` finds the needle.

### Trimming

```c
rc_str rc_str_remove_prefix(rc_str s, rc_str prefix);
rc_str rc_str_remove_suffix(rc_str s, rc_str suffix);
```

Return `s` with the prefix or suffix removed when present, otherwise `s`
unchanged.

### Splitting

```c
rc_str_pair rc_str_first_split(rc_str s, rc_str split_by);
rc_str_pair rc_str_last_split(rc_str s, rc_str split_by);
```

Split `s` at the first (or last) occurrence of `split_by`. `.first` is the text
before the delimiter and `.second` is the text after it; neither includes the
delimiter. When the delimiter is absent, `.first` is the whole input and
`.second` is the invalid view (test with `rc_str_is_valid(pair.second)`).

### Conversion

```c
const char *rc_str_as_cstr(rc_str s, char *buf, uint32_t buf_size);
```

Return a null-terminated C string for `s`.

- Fast path: when `s.data` is non-NULL and already followed by a `'\0'` byte,
  `s.data` is returned directly with no copy.
- Slow path: otherwise up to `buf_size - 1` bytes are copied into `buf` and a
  `'\0'` is appended; `buf` is returned. The copy is truncated to fit.
- Returns NULL when no fast path applies and `buf` is NULL or `buf_size` is 0.

---

## richc/test.h - unit-test framework

A self-registering unit-test framework. Each test macro places a pointer to a
test descriptor into a dedicated linker section, and the runner walks that
section at run time; no manual registration list is required. The framework is
part of the core `richc` library and is only linked into a final executable
when something references `rc_test_run`.

Supported platforms: Windows (MSVC, clang, gcc) and Linux (clang, gcc).

### Defining tests

```c
RC_TEST(group, name) { ... }        // a simple test
RC_TEST_SKIP(group, name) { ... }   // registered but not run (reported SKIP)
```

`group` and `name` are bare identifiers, used to label the test as
`group.name`. The body that follows the macro is the test function.

### Fixtures

```c
RC_TEST_GROUP_DATA(group) { ... };  // declare the per-group fixture struct
RC_TEST_GROUP_INIT(group) { ... }   // runs before each test in the group
RC_TEST_GROUP_DEINIT(group) { ... } // runs after each test in the group
RC_TEST_STEP(group, name) { ... }   // a test that receives the fixture
RC_TEST_STEP_SKIP(group, name)      // registered fixtured test, not run
```

Declare the fixture struct with `RC_TEST_GROUP_DATA`, then define `INIT` and
`DEINIT` for the group. Inside the `INIT`, `DEINIT`, and `STEP` bodies the
fixture is available as the local pointer `data`, of type
`struct rc_test_group_data_<group> *`. `INIT` runs before, and `DEINIT` after,
each `RC_TEST_STEP` in the group.

```c
RC_TEST_GROUP_DATA(counter) { int value; };
RC_TEST_GROUP_INIT(counter)        { data->value = 100; }
RC_TEST_GROUP_DEINIT(counter)      { data->value = 0; }
RC_TEST_STEP(counter, starts_full) { RC_CHECK(data->value, ==, 100); }
```

### Assertions

```c
RC_CHECK(a, op, b);   // assert a op b
RC_CHECK_TRUE(a);     // assert a is truthy
RC_CHECK_FALSE(a);    // assert a is falsy
```

The left operand selects the comparison through `_Generic`. Supported operand
types and operators:

| Operand type | Operators |
|--------------|-----------|
| `bool` | `==` `!=` |
| `int8_t` .. `uint64_t` (all fixed-width integers) | `==` `!=` `<` `>` `<=` `>=` |
| `float`, `double` | `==` `!=` `<` `>` `<=` `>=` and `~=` |
| `rc_str` | `==` `!=` |

`~=` is an approximate float compare with a fixed epsilon of 0.0001. Plain
`int` / `unsigned` literals match the `int32_t` / `uint32_t` entries. A failing
assertion prints the file, line, expression, and actual value, then aborts the
current test (the runner records it as a failure and continues with the next
test).

### Running

```c
int rc_test_run(const char *filter);
RC_TEST_MAIN();
```

- `rc_test_run(filter)` runs every test whose group name starts with `filter`
  (pass `""` to run all), prints a per-test line and a summary, and returns the
  number of failed tests.
- `RC_TEST_MAIN()` emits a `main` that calls `rc_test_run` with the first
  command-line argument as the filter (or `""` when none is given). Place it
  once in a test executable.

```c
#include "richc/str.h"
#include "richc/test.h"

RC_TEST(str, basics)
{
    rc_str s = RC_STR("hello world");
    RC_CHECK(s.len, ==, 11u);
    RC_CHECK_TRUE(rc_str_starts_with(s, RC_STR("hello")));
}

RC_TEST_MAIN()
```
