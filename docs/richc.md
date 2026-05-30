# richc reference

Detailed reference for every public type, macro, and function in richc,
organised by header.

General conventions used throughout the library:

- All public types and functions are prefixed `rc_`; all public macros `RC_`.
  Everything is `snake_case`.
- Function naming by role:
  - `rc_<type>_make(...)` / `rc_<type>_make_<thing>(...)` acts like a
    constructor - it returns a fresh `rc_<type>` by value.
  - `rc_<type>_from_<other>(...)` acts like a conversion constructor - it builds
    an `rc_<type>` from a value of a different type.
  - `rc_<type>_as_<other>(...)` acts like a cast - it reinterprets the value as
    another type without allocating or copying.
  - `rc_<type>_is_<predicate>(...)` returns a `bool` (e.g. `is_valid`,
    `is_empty`).
- A trailing underscore denotes an internal (private) symbol that happens to be
  visible in a public header (e.g. `RC_CHECK_IMPL_`, a generated `_grow_`
  helper). Do not use these directly; they are not part of the API.
- The index type is `uint32_t`. `RC_INDEX_NONE` (`UINT32_MAX`) is the sentinel
  for "not found" / "invalid".
- Functions that allocate take an `rc_arena *`, named `arena`, as their last
  parameter. It may be NULL when no allocation is actually required; arena
  allocation never returns NULL (out of memory is a panic), so results are not
  NULL-checked.
- Preconditions are enforced with `RC_ASSERT` (active in debug builds): passing
  an invalid argument - a NULL pointer, an out-of-range index, an invalid view -
  is a programming error that traps, not something the function reports.

## Contents

Headers:

- [richc/arena.h - arena allocator](#richcarenah---arena-allocator)
- [richc/bytes.h - byte buffers](#richcbytesh---byte-buffers)
- [richc/file.h - file I/O](#richcfileh---file-io)
- [richc/hash.h - hashing](#richchashh---hashing)
- [richc/macros.h - preprocessor utilities and assertions](#richcmacrosh---preprocessor-utilities-and-assertions)
- [richc/mstr.h - mutable string](#richcmstrh---mutable-string)
- [richc/ops.h - scalar bit and math operations](#richcopsh---scalar-bit-and-math-operations)
- [richc/str.h - string view](#richcstrh---string-view)
- [richc/test.h - unit-test framework](#richctesth---unit-test-framework)

Templates:

- [richc/template/array.h - view, span, array](#richctemplatearrayh---view-span-array)

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

## richc/bytes.h - byte buffers

`bytes.h` instantiates the [array template](#richctemplatearrayh---view-span-array)
for `uint8_t`, giving the byte container family:

```c
rc_view_bytes    // { const uint8_t *data; uint32_t num; }
rc_span_bytes    // {       uint8_t *data; uint32_t num; }
rc_array_bytes   // {       uint8_t *data; uint32_t num; uint32_t cap; }
```

Every view/span/array operation applies, named `rc_array_bytes_*`,
`rc_span_bytes_*`, and `rc_view_bytes_*` (e.g. `rc_array_bytes_push`,
`rc_view_bytes_get_subview`). Include the header once.

---

## richc/file.h - file I/O

Whole-file load and save. Filenames are `rc_str`; all I/O is binary mode (no
line-ending translation), and loaded data is allocated from the supplied arena.
Every function reports `rc_file_error`: `RC_FILE_OK` (0) on success, otherwise
`RC_FILE_ERROR_NOT_FOUND`, `RC_FILE_ERROR_ACCESS_DENIED`,
`RC_FILE_ERROR_TOO_LARGE`, or `RC_FILE_ERROR_IO`.

### Loading

Each load has an immutable and a mutable form. The mutable form takes a
`minimum_capacity` so the returned `rc_mstr` / `rc_array_bytes` can grow before
reallocating.

```c
typedef struct { rc_str         text; rc_file_error error; } rc_load_text_result;
typedef struct { rc_mstr        text; rc_file_error error; } rc_load_text_mut_result;
typedef struct { rc_view_bytes  data; rc_file_error error; } rc_load_binary_result;
typedef struct { rc_array_bytes data; rc_file_error error; } rc_load_binary_mut_result;

rc_load_text_result       rc_load_text(rc_str filename, rc_arena *arena);
rc_load_text_mut_result   rc_load_text_mut(rc_str filename, uint32_t minimum_capacity, rc_arena *arena);
rc_load_binary_result     rc_load_binary(rc_str filename, rc_arena *arena);
rc_load_binary_mut_result rc_load_binary_mut(rc_str filename, uint32_t minimum_capacity, rc_arena *arena);
```

Text loads are always null-terminated, so `rc_str_as_cstr` on the result takes
its no-copy fast path. On failure the returned text/data is the empty (invalid)
state and `error` is set.

### Saving

```c
rc_file_error rc_save_text(rc_str filename, rc_str text);
rc_file_error rc_save_binary(rc_str filename, rc_view_bytes data);
```

Both create or truncate the file.

---

## richc/hash.h - hashing

`uint32_t` hash functions for richc types, suitable as the hash expression for
the hash-table templates.

```c
uint32_t rc_hash_u32(uint32_t x);          // Murmur3 32-bit finalizer
uint32_t rc_hash_i32(int32_t x);
uint32_t rc_hash_u64(uint64_t x);          // splitmix64 finalizer, folded to 32 bits
uint32_t rc_hash_i64(int64_t x);
uint32_t rc_hash_f32(float x);             // by bit pattern; -0.0f and +0.0f hash alike
uint32_t rc_hash_f64(double x);
uint32_t rc_hash_ptr(const void *p);       // hashes the pointer, not the pointee
uint32_t rc_hash_bytes(const void *data, uint32_t len);   // FNV-1a 32-bit
uint32_t rc_hash_str(rc_str s);            // hashes the string's bytes
uint32_t rc_hash_combine(uint32_t seed, uint32_t hash);   // Boost hash_combine
```

`rc_hash_combine` mixes one hash into a running seed, for hashing a struct field
by field:

```c
uint32_t h = rc_hash_i32(point.x);
h = rc_hash_combine(h, rc_hash_i32(point.y));
```

Float values `-0.0` and `+0.0` are equal under `==`, so they are normalised to
hash the same. Hash functions for further types are added here as those types
are ported.

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

## richc/mstr.h - mutable string

`rc_mstr` is an arena-backed growable string. Its `{ data, len }` fields share
layout with `rc_str` and are exposed as `s.view`, so a non-owning view of the
current contents is always available without copying. The buffer always holds a
`'\0'` at `data[len]`, so `rc_str_as_cstr(s.view, ...)` takes the no-copy fast
path.

### Type

```c
typedef struct rc_mstr {
    union {
        struct { const char *data; uint32_t len; };
        rc_str view;
    };
    uint32_t cap;
} rc_mstr;
```

`cap` is the character capacity, excluding the null terminator; the backing
allocation is always `cap + 1` bytes. A zeroed `rc_mstr` is the invalid state
(`{ NULL, 0, 0 }`); a valid one has non-NULL `data` and `len <= cap`.

### Construction

```c
rc_mstr rc_mstr_make(uint32_t cap, rc_arena *a);
rc_mstr rc_mstr_from_cstr(const char *s, uint32_t max_cap, rc_arena *a);
rc_mstr rc_mstr_from_str(rc_str s, uint32_t max_cap, rc_arena *a);
```

- `rc_mstr_make` returns an empty string with the given initial capacity.
- `rc_mstr_from_cstr` / `rc_mstr_from_str` copy the source, sizing the buffer to
  `max(source length, max_cap)`. They return the invalid state when given a NULL
  C string or an invalid `rc_str`.

### Predicates

```c
bool rc_mstr_is_valid(const rc_mstr *s);   // inline; true when data is non-NULL
bool rc_mstr_is_empty(const rc_mstr *s);   // inline; true when len is 0
```

### Mutation

```c
void rc_mstr_reset(rc_mstr *s);
void rc_mstr_reserve(rc_mstr *s, uint32_t new_cap, rc_arena *a);
void rc_mstr_append(rc_mstr *s, rc_str str, rc_arena *a);
void rc_mstr_append_char(rc_mstr *s, char c, rc_arena *a);
void rc_mstr_replace(rc_mstr *s, rc_str find, rc_str replacement, rc_arena *a);
```

- `rc_mstr_reset` sets `len` to 0 and keeps the buffer.
- `rc_mstr_reserve` ensures capacity for at least `new_cap` characters (no-op if
  already large enough); it may move the buffer.
- `rc_mstr_append` / `rc_mstr_append_char` append, growing by doubling (minimum
  8) as needed; appending an empty `rc_str` is a no-op.
- `rc_mstr_replace` replaces every non-overlapping occurrence of `find` with
  `replacement`, rewriting in place (left-to-right when the result is no larger,
  otherwise reserving and rewriting right-to-left). An empty `find` is a no-op.

The mutation functions require a valid `rc_mstr` and valid `rc_str` arguments
(asserted), and allocate through the arena, which never returns NULL.

---

## richc/ops.h - scalar bit and math operations

Small `static inline` scalar helpers. Functions carry a scalar type suffix
(`i32`/`i64`/`u32`/`u64`/`f32`/`f64`), which also avoids the Windows `min`/`max`
macros.

```c
uint32_t rc_bitcast_f32(float x);          // float  -> its uint32_t bit pattern
uint64_t rc_bitcast_f64(double x);         // double -> its uint64_t bit pattern

int32_t  rc_min_i32(int32_t a, int32_t b);     int32_t rc_max_i32(int32_t a, int32_t b);
int64_t  rc_min_i64(int64_t a, int64_t b);     int64_t rc_max_i64(int64_t a, int64_t b);
int32_t  rc_sgn_i32(int32_t a);                int64_t rc_sgn_i64(int64_t a);   // -1, 0, or +1

int32_t  rc_gcd_i32(int32_t a, int32_t b);     // Euclidean GCD, always non-negative
int64_t  rc_gcd_i64(int64_t a, int64_t b);

uint32_t rc_clz_u32(uint32_t a);           // count leading zeros (32 for a == 0)
uint32_t rc_clz_u64(uint64_t a);           // count leading zeros (64 for a == 0)

bool rc_mul_overflows_u64(uint64_t a, uint64_t b);   bool rc_add_overflows_u64(uint64_t a, uint64_t b);
bool rc_add_overflows_i64(int64_t a, int64_t b);     bool rc_sub_overflows_i64(int64_t a, int64_t b);
bool rc_mul_overflows_i64(int64_t a, int64_t b);

float rc_deg_to_rad(float degrees);
```

The overflow checks return true when the operation would overflow the result
type. `rc_bitcast_f32` / `rc_bitcast_f64` reinterpret the float bits (via a
union); they are used by the float hashes.

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

---

## richc/template/array.h - view, span, array

A template header that, for one element type, generates a read-only view, a
mutable span, and a growable arena-backed array.

### Instantiation

Define `RC_ARRAY_TYPE` (and optionally `RC_ARRAY_NAME`) before including:

```c
#define RC_ARRAY_TYPE int
#include "richc/template/array.h"   // rc_view_int, rc_span_int, rc_array_int
```

The generated types are `rc_array_<s>`, `rc_span_<s>`, `rc_view_<s>`, where `<s>`
is `RC_ARRAY_NAME` (default: the element type's spelling, so it must be a single
identifier - give a name for multi-token types). Both control macros are
undefined again by the header, so it can be included again for another type.

### Types

```c
typedef struct rc_view_<s>  { const T *data; uint32_t num; }              rc_view_<s>;
typedef struct rc_span_<s>  {       T *data; uint32_t num; }              rc_span_<s>;
typedef struct rc_array_<s> {       T *data; uint32_t num; uint32_t cap; } rc_array_<s>;
```

The span and array embed an anonymous union, so converting to a narrower type is
a typesafe field access with no function call:

```c
rc_view_int v = arr.view;    // array -> view
rc_span_int s = arr.span;    // array -> span
rc_view_int w = s.view;      // span  -> view
```

### Shared macros (defined once, work on any view/span/array)

- `RC_AT(c, i)` - bounds-checked element access; an lvalue, so it can be read or
  assigned. Asserts `i < c.num`.
- `RC_VIEW(arr)` / `RC_SPAN(arr)` - brace initializers from a C array
  expression, for use in a declaration only. The count is derived via `sizeof`,
  so `arr` must be a real array, not a pointer.

```c
int raw[] = {1, 2, 3};
rc_view_int v = RC_VIEW(raw);
rc_span_int s = RC_SPAN(raw);
```

### Array operations

Construction and access:

```c
rc_array_<s> rc_array_<s>_make(uint32_t initial_capacity, rc_arena *arena);
rc_array_<s> rc_array_<s>_make_copy(rc_view_<s> view, uint32_t minimum_capacity, rc_arena *arena);
T            rc_array_<s>_get(const rc_array_<s> *array, uint32_t index);   // by value
void         rc_array_<s>_set(rc_array_<s> *array, uint32_t index, T value);
T           *rc_array_<s>_at(rc_array_<s> *array, uint32_t index);
bool         rc_array_<s>_is_valid(const rc_array_<s> *array);   // owns a buffer (data != NULL)
bool         rc_array_<s>_is_empty(const rc_array_<s> *array);   // num == 0
```

Capacity and size:

```c
void         rc_array_<s>_reserve(rc_array_<s> *array, uint32_t capacity, rc_arena *arena); // exact
rc_span_<s>  rc_array_<s>_resize(rc_array_<s> *array, uint32_t num, rc_arena *arena);        // -> whole array
void         rc_array_<s>_reset(rc_array_<s> *array);                                       // num = 0
```

`reserve` allocates the exact capacity requested. The growing operations below
instead grow geometrically (to the larger of double the capacity, the request,
or 8), so they stay amortised O(1). `resize` sets the element count and returns
a span over the whole array - the idiom is to resize to a fixed size, then work
through the returned span.

Adding and removing:

```c
uint32_t rc_array_<s>_push(rc_array_<s> *array, T value, rc_arena *arena);           // -> index
uint32_t rc_array_<s>_push_n(rc_array_<s> *array, uint32_t n, rc_arena *arena);      // n uninitialised; -> first index
uint32_t rc_array_<s>_push_n_zero(rc_array_<s> *array, uint32_t n, rc_arena *arena); // n zeroed; -> first index
T        rc_array_<s>_pop(rc_array_<s> *array);                                      // last by value; asserts non-empty
void     rc_array_<s>_pop_n(rc_array_<s> *array, uint32_t n);                        // drop last n
uint32_t rc_array_<s>_append(rc_array_<s> *array, rc_view_<s> view, rc_arena *arena);// -> new element count
void     rc_array_<s>_insert(rc_array_<s> *array, uint32_t index, T value, rc_arena *arena);
void     rc_array_<s>_insert_n(rc_array_<s> *array, uint32_t index, uint32_t n, rc_arena *arena);      // uninitialised
void     rc_array_<s>_insert_n_zero(rc_array_<s> *array, uint32_t index, uint32_t n, rc_arena *arena); // zeroed
void     rc_array_<s>_remove(rc_array_<s> *array, uint32_t index);                   // shift tail left
void     rc_array_<s>_remove_n(rc_array_<s> *array, uint32_t index, uint32_t n);
```

### Span operations

A span is passed by value; `set`/`at`/`last_at` write through to the underlying
memory.

```c
rc_span_<s> rc_span_<s>_make(T *data, uint32_t num);            // wraps a pointer; no allocation
bool        rc_span_<s>_is_valid(rc_span_<s> span);            // data != NULL
bool        rc_span_<s>_is_empty(rc_span_<s> span);            // num == 0
rc_span_<s> rc_span_<s>_get_subspan(rc_span_<s> span, uint32_t start, uint32_t end);  // [start, end), clamped
rc_span_<s> rc_span_<s>_get_head(rc_span_<s> span, uint32_t n);  // first n, clamped
rc_span_<s> rc_span_<s>_get_tail(rc_span_<s> span, uint32_t n);  // from index n, clamped
T           rc_span_<s>_get(rc_span_<s> span, uint32_t index);   // by value
void        rc_span_<s>_set(rc_span_<s> span, uint32_t index, T value);
T          *rc_span_<s>_at(rc_span_<s> span, uint32_t index);
T          *rc_span_<s>_last_at(rc_span_<s> span);               // asserts non-empty
```

### View operations

The same set as span, minus `set` (read-only); `at`/`last_at` return `const`
pointers, and the slice helpers take and return views.

```c
rc_view_<s> rc_view_<s>_make(const T *data, uint32_t num);      // wraps a pointer; no allocation
bool        rc_view_<s>_is_valid(rc_view_<s> view);
bool        rc_view_<s>_is_empty(rc_view_<s> view);
rc_view_<s> rc_view_<s>_get_subview(rc_view_<s> view, uint32_t start, uint32_t end);  // clamped
rc_view_<s> rc_view_<s>_get_head(rc_view_<s> view, uint32_t n);  // clamped
rc_view_<s> rc_view_<s>_get_tail(rc_view_<s> view, uint32_t n);  // clamped
T               rc_view_<s>_get(rc_view_<s> view, uint32_t index);
const T        *rc_view_<s>_at(rc_view_<s> view, uint32_t index);
const T        *rc_view_<s>_last_at(rc_view_<s> view);           // asserts non-empty
```

There is no allocating span/view constructor: allocation is the array's job. For
a fixed-size allocated span, make an array and resize it
(`rc_array_<s>_resize(&a, n, arena)` returns the span); for a copy, use
`rc_array_<s>_make_copy(view, 0, arena)` and take its `.span` / `.view`.

### Arena parameter

Every operation that may reallocate takes `rc_arena *arena` last. Passing NULL
is valid when no growth is required; a reallocation with `arena == NULL` asserts.
Arena allocation never returns NULL (out of memory is a panic), so results are
never NULL-checked.
