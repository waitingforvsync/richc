# richc reference

Detailed reference for every public type, macro, and function in richc,
organised by header.

General conventions used throughout the library:

- All public types and functions are prefixed `rc_`; all public macros `RC_`.
- The index type is `uint32_t`. `RC_INDEX_NONE` (`UINT32_MAX`) is the sentinel
  for "not found" / "invalid".
- Functions that allocate take an `rc_arena *` as their last parameter (none of
  the headers documented so far allocate).

## Contents

- [richc/arena.h - arena allocator](#richcarenah---arena-allocator)
- [richc/macros.h - preprocessor utilities and assertions](#richcmacrosh---preprocessor-utilities-and-assertions)
- [richc/str.h - string view](#richcstrh---string-view)
- [richc/test.h - unit-test framework](#richctesth---unit-test-framework)

---

## richc/arena.h - arena allocator

`rc_arena` is a virtual-memory-backed stack (bump) allocator. It reserves a
large region of address space up front and commits physical pages on demand.
Because the reserved range never moves, pointers into the arena stay valid for
its entire lifetime. Every allocation is aligned to `RC_MAX_ALIGN` (the
alignment of `max_align_t`), so it suits any standard C type.

### Type and constants

```c
typedef struct rc_arena {
    char     *base;       // base of the reserved region; first allocation address
    uint32_t  top;        // offset of the next free byte (always RC_MAX_ALIGN-aligned)
    uint32_t  committed;  // offset one past the last committed byte
    uint32_t  reserved;   // total reserved bytes
} rc_arena;

#define RC_MAX_ALIGN              // alignment applied to every allocation
#define RC_ARENA_DEFAULT_RESERVE  // 256 MB; the default reservation size
```

Offsets are `uint32_t`, so a single arena spans at most 4 GB. A zeroed
`rc_arena` (all fields 0) is the failure/empty value; check `base` after
creation.

### Lifetime

```c
rc_arena rc_arena_make(uint32_t reserve_size);
rc_arena rc_arena_make_default(void);   // inline; rc_arena_make(RC_ARENA_DEFAULT_RESERVE)
void     rc_arena_destroy(rc_arena *a);
```

`rc_arena_make` reserves `reserve_size` bytes (rounded up to a page) and commits
the first page. On failure it returns a zeroed arena, so test `a.base` - this is
the one place arena setup may fail and be handled. `rc_arena_destroy` releases
all virtual memory and zeroes the struct.

### Allocation

```c
void *rc_arena_alloc(rc_arena *a, uint32_t size);
void *rc_arena_alloc_zero(rc_arena *a, uint32_t size);
```

Both bump-allocate `size` bytes. They **never return NULL** - running out of
space is a fatal `RC_PANIC`, not a NULL return, so callers must not NULL-check
the result (unlike idiomatic `malloc`/`realloc` code). `size` must be non-zero
and `a` must be a created arena; both are asserted. `rc_arena_alloc` does NOT
zero its memory; `rc_arena_alloc_zero` does. Freshly committed pages are
OS-zeroed, but space reclaimed by a free retains its old contents, so use the
zeroing variant whenever a clean buffer is required.

Convenience macros allocate `n` elements of a type:

```c
#define rc_arena_alloc_type(arena, T, n)       // ((T *)rc_arena_alloc(...))
#define rc_arena_alloc_zero_type(arena, T, n)  // zeroed variant
```

### Freeing

```c
bool rc_arena_free(rc_arena *a, void *ptr, uint32_t size);
void rc_arena_free_to(rc_arena *a, uint32_t offset);   // inline
```

`rc_arena_free` only succeeds (returns true) when `ptr` is the most recent
allocation, in which case it moves `top` back; otherwise it is a no-op returning
false (interior space cannot be reclaimed). `rc_arena_free_to` resets `top` to
an earlier `offset`, freeing everything allocated after that point in O(1):

```c
uint32_t mark = a.top;
// ... allocate temporaries ...
rc_arena_free_to(&a, mark);   // discard them all at once
```

### Reallocation

```c
void *rc_arena_realloc(rc_arena *a, void *ptr, uint32_t old_size, uint32_t new_size);
void *rc_arena_realloc_zero(rc_arena *a, void *ptr, uint32_t old_size, uint32_t new_size);
```

Resize the allocation at `ptr`. A NULL `ptr` behaves like `alloc`. When `ptr` is
the last allocation it grows or shrinks in place; otherwise growing copies the
data to a fresh allocation and shrinking is a no-op. `rc_arena_realloc` does not
zero new bytes when growing; `rc_arena_realloc_zero` does. Like the allocation
functions, these never return NULL (out of space is an `RC_PANIC`) and assert
that `new_size` is non-zero.

### Reset

```c
void rc_arena_reset(rc_arena *a);
```

Return `top` to 0 and decommit every page except the first, handing physical
memory back to the OS. The arena stays valid and ready for reuse.

### Scratch pattern

Passing an `rc_arena` by value gives the callee a snapshot of the bump pointer.
Allocations inside the callee advance only the local copy, so the caller's arena
is unchanged on return:

```c
void build_temp(rc_arena scratch, rc_arena *out) {
    int *tmp    = rc_arena_alloc_type(&scratch, int, 1024);  // local only
    int *result = rc_arena_alloc_type(out, int, n);          // survives
}
```

---

## richc/macros.h - preprocessor utilities and assertions

Small general-purpose preprocessor helpers used across the library.

```c
#define RC_CONCAT(a, b)   // paste two tokens together, expanding macros first
#define RC_STRINGIFY(x)   // convert a token to a string literal, expanding macros first
#define RC_INDEX_NONE     // sentinel "not found" / "invalid" index (== UINT32_MAX)
#define RC_ASSERT(cond)   // debug-only assertion; breaks into the debugger on failure
#define RC_PANIC(cond)    // always-active assertion; traps on failure
```

- `RC_CONCAT(a, b)` expands its arguments and then token-pastes them, so
  `RC_CONCAT(rc_array_, int)` yields `rc_array_int`.
- `RC_STRINGIFY(x)` expands its argument and then stringizes it, so
  `RC_STRINGIFY(RC_INDEX_NONE)` yields `"((uint32_t)-1)"`.
- `RC_INDEX_NONE` is the `uint32_t` value used throughout the library to mean
  "no such index".
- `RC_ASSERT(cond)` checks `cond` in debug builds and triggers a debug break if
  it is false; under `NDEBUG` it evaluates and discards `cond` (so variables
  used only in assertions do not warn). It is an expression, not a statement, so
  it can sit on the left of a comma operator.
- `RC_PANIC(cond)` checks `cond` in all builds and traps (terminates) on
  failure. Use it for unrecoverable invariants such as out-of-memory.

---

## richc/str.h - string view

`rc_str` is a non-owning view over character data: a pointer and a length. It is
used like a value type - passed and held by value - and never allocates. The
invalid view (`{ NULL, 0 }`) is a "not found" / "absent" sentinel: test for it
with `rc_str_is_valid` rather than passing it on. Functions that operate on the
string (comparison, search, conversion) assert their `rc_str` arguments are
valid; the empty-but-valid view (`{ ptr, 0 }`) is fully supported.

### Types

```c
typedef struct rc_str { const char *data; uint32_t len; } rc_str;
typedef struct rc_str_pair { rc_str first; rc_str second; } rc_str_pair;
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
RC_TEST_GROUP_DATA(group) { ... };       // declare the per-group fixture struct
RC_TEST_GROUP_INIT(group, fix) { ... }   // runs before each test in the group
RC_TEST_GROUP_DEINIT(group, fix) { ... } // runs after each test in the group
RC_TEST_STEP(group, name, fix) { ... }   // a test that receives the fixture
RC_TEST_STEP_SKIP(group, name, fix)      // registered fixtured test, not run
```

Declare the fixture struct with `RC_TEST_GROUP_DATA`, then define `INIT` and
`DEINIT` for the group. The last argument to `INIT`, `DEINIT`, and `STEP` names
the fixture pointer made available inside the body; it has type
`struct rc_test_group_data_<group> *`. `INIT` runs before, and `DEINIT` after,
each `RC_TEST_STEP` in the group.

```c
RC_TEST_GROUP_DATA(counter) { int value; };
RC_TEST_GROUP_INIT(counter, fix)        { fix->value = 100; }
RC_TEST_GROUP_DEINIT(counter, fix)      { fix->value = 0; }
RC_TEST_STEP(counter, starts_full, fix) { RC_CHECK(fix->value, ==, 100); }
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
