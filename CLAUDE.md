# richc — Project Notes for Claude Code

## What this is
A header-only C library providing template-based generic data structures and
algorithms. Pure C, no external dependencies. Designed for high performance:
cache-friendly layouts, arena allocation, compile-time code generation via the
C preprocessor.

## Header guards
Never use `#pragma once`. All headers use the `#ifndef` include-guard idiom.
The guard macro is named `RC_<HEADER_NAME>_H_`, where `<HEADER_NAME>` is the
filename uppercased with dots replaced by underscores.

Examples: `RC_ARENA_H_`, `RC_PLATFORM_H_`, `RC_DEBUG_H_`, `RC_TEMPLATE_UTIL_H_`.

Template headers (e.g. `array.h`, `sort.h`) are intentionally included multiple
times for different types, so they must **not** have an include guard.

## Formatting
- **Compound literals:** write a space between the closing `)` of the type and
  the opening `{`. No space after `{` or before `}`.
  ```c
  return (rc_str) {s.data, count};
  return (rc_str_pair) {
      rc_str_left(s, pos),
      rc_str_from(s, pos + split_by.len)
  };
  ```

## Ground rules
- **Language standard: C17** — compile with `-std=c17`
- **Compiler: clang 20** — `C:\clang\bin\clang.exe`
- **Build tools** (all discovered under the VS 2022 install):
  - CMake 3.31: `C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe`
  - Ninja 1.12: `C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe`
- **Always write exhaustive tests** for new code, including difficult edge
  cases (empty containers, single element, duplicate values, max-size inputs,
  NULL/zero inputs where applicable, etc.)
- **Always run the full test suite** after every change and verify it passes
  before considering work done. Nothing is done until the tests are green.
- **Build commands** — run from `C:\Users\richard.talbotwatkin\richc\`.
  Use the full paths above; these tools are not on PATH in the bash shell.
  Always pass `-DCMAKE_C_COMPILER` so clang is used (not MSVC):
  ```
  "C:/Program Files/Microsoft Visual Studio/2022/Professional/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe" \
      -B build -G Ninja \
      -DCMAKE_C_COMPILER=C:/clang/bin/clang.exe \
      -DCMAKE_MAKE_PROGRAM="C:/Program Files/Microsoft Visual Studio/2022/Professional/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe"

  "C:/Program Files/Microsoft Visual Studio/2022/Professional/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe" \
      --build build
  ```
  Tests run automatically as a post-build step. A failed assertion causes the
  build to report an error.

## Naming conventions
- **All snake_case.** No CamelCase anywhere in the library.
- **Library prefix `rc_` on all public types and functions.**
  Examples: `rc_arena`, `rc_array_int_push()`, `rc_sort_int()`.
- **Library prefix `RC_` on all public macros.**
  Examples: `RC_ASSERT`, `RC_AT`, `RC_INDEX_NONE`, `RC_AS_VIEW`.
- **Internal (private) macros use a trailing `_`** in addition to the prefix:
  `RC_DEBUG_BREAK_`, `RC_ARRAY_ONCE_`, `RC_TRIE_ONCE_`.
- **Once-only sections in template headers** use the header-guard naming
  convention (`RC_<FILENAME>_H_`) rather than a bespoke `_ONCE_` suffix.
  Examples: `RC_HASH_MAP_H_` (guards `RC_MAP_ALIGN_UP_` and the
  `rc_hash_map_slot_state` enum in `hash_map.h`), `RC_HASH_SET_H_` (guards
  `RC_SET_ALIGN_UP_` and the `rc_hash_set_slot_state` enum in `hash_set.h`).
- **Template control macros** (`ARRAY_T`, `SORT_CMP`, `FIND_PRED`, etc.) are
  *consumed* at include-time and leave no compiled trace. They stay unprefixed
  ALL_CAPS — adding `RC_` would only add noise at every instantiation site.
- **Template type naming: container-first.**
  `rc_array_int`, `rc_view_int`, `rc_span_int`, `rc_map_int`, `rc_trie_uint64_t`.
  Functions follow the same order: `rc_array_int_push()`, `rc_sort_int()`,
  `rc_lower_bound_int()`.
- **Arena parameter placement:** functions that may allocate accept `rc_arena *`
  as their *last* parameter. If a function also needs private scratch space, it
  additionally accepts an `rc_arena` passed *by value* as a further parameter
  after the `rc_arena *` (the scratch arena is consumed locally and discarded).
- **Index type:** always `uint32_t`, never `size_t`. Prefer returning or
  passing an index over a pointer whenever that makes sense (e.g. insertion
  position, search result). Use `UINT32_MAX` / `RC_INDEX_NONE` as the sentinel
  "not found" / "invalid" index value.
- **Function naming by role** (three-part convention):
  - `rc_<type>_make(...)` / `rc_<type>_make_<thing>(...)` — constructors that
    return a value of `rc_<type>` by value. E.g. `rc_arena_make()`,
    `rc_mat44f_make_identity()`, `rc_mstr_make_from_cstr()`.
  - `rc_<type>_from_<other>(...)` — construct an `rc_<type>` from a value of a
    *different* type. Brevity variant of `make_from_<other>` — equally correct
    but shorter. E.g. `rc_mstr_from_str()`, `rc_mat44f_from_mat33f()`.
  - `rc_<type>_as_<other>(...)` — cast or reinterpret the current value as
    another type without allocating. E.g. `rc_array_int_as_view()`,
    `rc_mat44f_as_floats()`.

## Architecture

### Memory model
**Arenas are the only allocation primitive.**  The library never calls `malloc`,
`realloc`, or `free` directly.  All dynamic allocation goes through `rc_arena`.

All allocating operations accept an `rc_arena *` parameter.
- `arena.h` / `arena.c` — virtual-memory-backed bump allocator (`rc_arena`);
  reserves 256 MB of VA space upfront, commits pages on demand.
- `platform.h` — thin OS abstraction: `rc_platform_reserve`, `rc_platform_commit`,
  `rc_platform_decommit`, `rc_platform_release`, `rc_platform_page_size`. Supports
  Windows (`VirtualAlloc`/`VirtualFree`) and POSIX (`mmap`/`munmap`).

### Template pattern
Each header is a preprocessor template. The caller `#define`s one or more
control macros, then `#include`s the header. The header generates type-specific
code, then `#undef`s all its macros so the same header can be included again
for a different type.

Example:
```c
#define ARRAY_T int
#include "richc/template/array.h"   // produces rc_view_int, rc_span_int, rc_array_int + operations
```

Default naming is **container-first**: `rc_view_T`, `rc_span_T`, `rc_array_T`
(where T = the type name supplied via `ARRAY_T`).

Custom names can be supplied via optional `ARRAY_NAME`, `ARRAY_VIEW`, `ARRAY_SPAN`
macros before the include.

### Utility headers
- `template_util.h` — `RC_CONCAT(a, b)` token-paste helper; `RC_STRINGIFY(x)` stringification helper
- `debug.h` — `RC_ASSERT(cond)`: expression-form assert, triggers a debug
  break (not abort) on failure; no-op when `NDEBUG` is defined.
  `RC_PANIC(cond)`: always-active assertion (never stripped by `NDEBUG`);
  triggers `__builtin_trap()` — use for invariants that are unrecoverable
  in production too (e.g. arena OOM).

## Data structures

### String view
Header: `str.h` (not a template; include once).

```c
rc_str       { const char *data; uint32_t len; }
rc_str_pair  { rc_str first; rc_str second; }
```

Two distinct states: **invalid** `{ NULL, 0 }` (sentinel for "not found") and
**valid** (data non-NULL, len ≥ 0).

Construction: `RC_STR("literal")` (compile-time, literal only) or
`rc_str_make(const char *)` (run-time, NULL-safe).

Operations: `is_valid`, `is_empty`, `is_equal`, `is_equal_insensitive`,
`compare`, `compare_insensitive`, `left`, `right`, `substr`, `from`,
`starts_with`, `ends_with`, `find_first`, `find_last`, `contains`,
`remove_prefix`, `remove_suffix`, `first_split`, `last_split`, `as_cstr`.

All slicing functions clamp out-of-range indices rather than asserting.
Split functions return `{ s, invalid }` when the delimiter is absent.

### Slices (non-owning)
| Default type name | Layout |
|-------------------|--------|
| `rc_view_T` | `{ const T *data; uint32_t num; }` |
| `rc_span_T` | `{ T *data; uint32_t num; }` |

All three types (view, span, array) are generated by a single `#include "array.h"`.

Bounds-checked element access via `RC_AT(container, index)` (defined once in
`array.h`); aliases `RC_VIEW_AT` and `RC_SPAN_AT` also available.

Brace-enclosed initializers from a C array (initialisation context only, not
function arguments; `arr` must be a true array, not a pointer):
`RC_VIEW(arr)` — initialise any `rc_view_T` from `arr`.
`RC_SPAN(arr)` — initialise any `rc_span_T` from `arr`.

### Array (growable, arena-backed)
Header: `array.h`
Control macro: `#define ARRAY_T <type>`

Default type name: `rc_array_T { T *data; uint32_t num; uint32_t cap; }`

Operations: `push`, `pop`, `insert`, `remove`, `reserve`, `resize`.
Growth strategy: capacity doubles (minimum 8).

### Mutable string
Header: `mstr.h` (not a template; include once).

```c
rc_mstr  { union { struct { const char *data; uint32_t len; }; rc_str view; }; uint32_t cap; }
```

Arena-backed growable string.  `data`/`len` share layout with `rc_str`, so
the current contents are always accessible as `s.view` without copying.
The buffer always contains `'\0'` at `data[len]` (fast path for `rc_str_as_cstr`).

`cap` is the character capacity, not counting the null terminator; the
underlying allocation is always `cap + 1` bytes.

Construction (return by value):
- `rc_mstr_make(cap, arena)` — empty string with given initial capacity.
- `rc_mstr_make_from_cstr(s, max_cap, arena)` — copy of null-terminated string;
  `actual_cap = max(strlen(s), max_cap)`.
- `rc_mstr_make_copy(str, max_cap, arena)` — copy of `rc_str`;
  `actual_cap = max(str.len, max_cap)`.

Predicates (take `const rc_mstr *`): `is_valid`, `is_empty`.

Mutation (take `rc_mstr *`): `reset`, `reserve`, `append`, `append_char`, `replace`.

`replace` replaces all non-overlapping occurrences of `find` with `replacement`.
When `replacement` is no longer than `find`, the rewrite is in-place left-to-right
(no allocation); when it is longer, the buffer is first reserved to the new size and
then rewritten right-to-left in-place (no fresh buffer; arena pointer asserted non-NULL).

### Math types
Headers: `richc/math/` (all header-only except `mat44f`).

All types follow the `rc_` prefix convention and drop the `_t` suffix.
All inline functions carry an explicit type suffix to avoid Windows macro conflicts
(`near`/`far`/`min`/`max` are reserved).

| Header | Type | Notes |
|--------|------|-------|
| `math/math.h` | — | `rc_deg_to_rad`, `rc_min/max/sgn_i32/i64`, `rc_gcd_i32/i64`, `rc_clz_u32/u64`, `rc_mul/add_overflows_u64`, `rc_add/sub/mul_overflows_i64` |
| `math/vec2i.h` | `rc_vec2i` | 2D integer vector; `{int32_t x, y}`; `dot`/`wedge`/`lengthsqr` return `int64_t` |
| `math/vec3i.h` | `rc_vec3i` | 3D integer vector; `{int32_t x, y, z}`; `dot`/`lengthsqr` return `int64_t`; `cross` asserts result fits in `int32_t` |
| `math/vec2f.h` | `rc_vec2f` | 2D float vector |
| `math/vec3f.h` | `rc_vec3f` | 3D float vector; includes `from_vec3i` |
| `math/vec4f.h` | `rc_vec4f` | 4D float vector |
| `math/aabb2f.h` | `rc_aabb2f` | 2D float axis-aligned bounding box `{rc_vec2f min, max}` |
| `math/aabb2i.h` | `rc_aabb2i` | 2D integer axis-aligned bounding box `{rc_vec2i min, max}` |
| `math/mat22f.h` | `rc_mat22f` | 2×2 column-major float matrix; `{rc_vec2f cx, cy}` |
| `math/mat23f.h` | `rc_mat23f` | 2D affine transform; `{rc_mat22f m; rc_vec2f t}` |
| `math/mat33f.h` | `rc_mat33f` | 3×3 column-major float matrix; `{rc_vec3f cx, cy, cz}` |
| `math/mat34f.h` | `rc_mat34f` | 3D affine transform; `{rc_mat33f m; rc_vec3f t}`; includes `make_lookat` |
| `math/mat44f.h` | `rc_mat44f` | 4×4 column-major float matrix; `determinant`/`inverse` in `.c` |
| `math/quatf.h` | `rc_quatf` | Unit quaternion; `{rc_vec3f xyz; float w}`; Hamilton convention |
| `math/rational.h` | `rc_rational` | Rational arithmetic; `{int64_t num, denom}`; always canonical |
| `math/bigint.h` | `rc_bigint` | Arbitrary-precision integer; sign-magnitude, arena-backed; `uint32_t` limbs with `uint64_t` intermediates for carry/borrow/mul; 3-address API (result, b, c) for add/sub/mul/divmod/div/mod; `int64_t` shortcut variants (int_add/sub/mul/divmod/div/mod) build a transient stack-backed bigint and delegate; add/sub share a unified kernel; schoolbook O(m×n) mul uses scratch arena (by value) for temp buffer; division via single-limb fast path or Knuth Algorithm D (base 2³²); trivial ops inline, rest in `bigint.c` |

Matrix storage is column-major throughout: `cx` is the first column, etc.
The `make_transpose(rx, ry, rz, ...)` constructors accept *row* vectors and store
them transposed — useful for specifying a matrix row-by-row.

`rc_mat34f` and `rc_mat23f` represent affine transforms applied as `m * v + t`.
`rc_mat34f_inverse` and `rc_mat23f_inverse` assume the linear part is invertible.

`rc_quatf_make_angle_axis` normalises the axis internally.
`rc_mat33f_from_quatf` and `rc_quatf_from_mat33f` form a roundtrip for any rotation.
`rc_quatf_vec3f_transform` uses the Rodrigues formula (no matrix allocation needed).

`rc_rational` is always in canonical form: `denom > 0`, `gcd(|num|, denom) == 1`.
`rc_rational_make(n, 0)` → invalid `{0, 0}`.  Arithmetic operations use GCD
pre-reduction to keep intermediate values small: `int_mul`/`mul` apply cross-GCD
reduction and yield directly canonical results; `add`/`sub` use the AHU algorithm
(`gcd(t, d)` where `d = gcd(denoms)` rather than `gcd(t, lcm)`); `int_div`/`div`
pre-reduce then call `rc_rational_make` only for sign normalisation.
Comparison (`compare`, `is_less_than`, `is_greater_than`) delegate to
`rc_rational_sub` so no non-standard types are needed.

### Hash functions
Header: `hash.h` (not a template; include once).

`static inline` functions returning `uint32_t`, suitable for use as `MAP_HASH` /
`SET_HASH` expressions in the hash_map and hash_set templates.

| Function | Input | Algorithm |
|----------|-------|-----------|
| `rc_hash_u32` | `uint32_t` | Murmur3 32-bit finalizer |
| `rc_hash_i32` | `int32_t` | → `rc_hash_u32` |
| `rc_hash_u64` | `uint64_t` | splitmix64 finalizer, XOR-folded to 32 bits |
| `rc_hash_i64` | `int64_t` | → `rc_hash_u64` |
| `rc_hash_f32` | `float` | bit-pattern → `rc_hash_u32`; −0/+0 normalised |
| `rc_hash_f64` | `double` | bit-pattern → `rc_hash_u64`; −0/+0 normalised |
| `rc_hash_ptr` | `const void *` | → `rc_hash_u64` |
| `rc_hash_bytes` | `const void *, uint32_t len` | FNV-1a 32-bit |
| `rc_hash_str` | `rc_str` | → `rc_hash_bytes`; NULL/invalid string safe |
| `rc_hash_vec2i` | `rc_vec2i` | combine(hash_i32(x), hash_i32(y)) |
| `rc_hash_vec3i` | `rc_vec3i` | combine chain over x, y, z |
| `rc_hash_vec2f` | `rc_vec2f` | combine(hash_f32(x), hash_f32(y)) |
| `rc_hash_vec3f` | `rc_vec3f` | combine chain over x, y, z |
| `rc_hash_vec4f` | `rc_vec4f` | combine chain over x, y, z, w |
| `rc_hash_combine` | `uint32_t seed, uint32_t hash` | Boost hash_combine formula |

`rc_hash_combine` is the building block for hashing structs field by field.

### Hash map
Header: `hash_map.h`
Open-addressing with linear probing, Structure-of-Arrays layout.
Default type name: `rc_map_T` (keyed on `MAP_KEY_T`).
Tombstone deletion. Load factor < 3/4; capacity always power-of-two.
Zero-initializable empty state.
Operations: `add`, `remove`, `find`, `contains`, `reserve`, `next`, `key_at`, `val_at`.
Iteration: `next(map, pos)` returns the next occupied slot index; `key_at(map, i)` and `val_at(map, i)` access the entry at that slot.

### Hash set
Header: `hash_set.h`
Open-addressing with linear probing, Structure-of-Arrays layout (states + keys only; no values).
Default type name: `rc_set_T` (keyed on `SET_KEY_T`).
Tombstone deletion. Load factor < 3/4; capacity always power-of-two.
Zero-initializable empty state.
Operations: `add`, `remove`, `contains`, `reserve`, `next`, `key_at`.
Iteration: `next(set, pos)` returns the next occupied slot index; `key_at(set, i)` accesses the key at that slot.

### Hash trie
Header: `hash_trie.h`
16-way (4 bits per level) radix trie over hash values.
Default type name: `rc_trie_T` (keyed on `TRIE_KEY_T`);
pool type `rc_trie_T_pool`, node type `rc_trie_T_node`.
Flat arena-backed node pool; multiple tries can share one pool.
Operations: `create`, `find`, `add`, `delete`, `pool_reserve`.
`pool_reserve(pool, min_blocks, arena)` pre-allocates the pool's backing array for at least `min_blocks` 16-node blocks without populating them.

## File I/O

Headers: `file.h` (includes `bytes.h`) / Implementation: `src/file.c`.

```c
rc_file_error              { RC_FILE_OK=0, RC_FILE_ERROR_NOT_FOUND,
                             RC_FILE_ERROR_ACCESS_DENIED, RC_FILE_ERROR_TOO_LARGE,
                             RC_FILE_ERROR_IO }
rc_load_text_result        { rc_str text; rc_file_error error; }
rc_load_binary_result      { rc_view_bytes data; rc_file_error error; }
rc_load_binary_array_result{ rc_array_bytes data; rc_file_error error; }
```

All file functions use binary mode (no line-ending translation). All load functions
use `fseek`/`ftell` to measure the file size, then allocate exactly the right amount
from the arena. A shared static `load_raw` helper keeps the three load functions DRY.

- `rc_load_text(filename, arena)` — reads entire file into arena memory; buffer is
  always null-terminated so `rc_str_as_cstr` on the result takes the fast path.
  Returns invalid `text` on failure.
- `rc_save_text(filename, text)` — writes `text` to file (create or truncate).
  Returns `rc_file_error`.
- `rc_load_binary(filename, arena)` — reads entire file; returns a read-only
  `rc_view_bytes`. Returns `{NULL, 0}` on failure.
- `rc_load_binary_array(filename, arena)` — reads entire file as a growable
  `rc_array_bytes` with `cap` set to the file size. Use `rc_array_uint8_t_push` etc.
  to mutate after loading. Returns `{0}` on failure.
- `rc_save_binary(filename, data)` — writes `rc_view_bytes` to file (create or
  truncate). Returns `rc_file_error`.

### bytes.h
Convenience include-guarded header that generates the `uint8_t` array family:
```c
rc_view_bytes  { const uint8_t *data; uint32_t num; }
rc_span_bytes  {       uint8_t *data; uint32_t num; }
rc_array_bytes       {       uint8_t *data; uint32_t num; uint32_t cap; }
```
Operation functions are named after `ARRAY_NAME`:
`rc_array_bytes_push`, `rc_array_bytes_push_n`, etc.

## Algorithms

| Header | Function | Notes |
|--------|----------|-------|
| `sort.h` | Introsort | Quicksort + heapsort fallback + insertion sort (n≤16) |
| `lower_bound.h` | First index where `arr[i] >= key` | Matches C++ `std::lower_bound` |
| `upper_bound.h` | First index where `arr[i] > key` | Matches C++ `std::upper_bound` |
| `min_element.h` | Index of first minimum element | Returns `RC_INDEX_NONE` if empty; optional context comparator |
| `max_element.h` | Index of first maximum element | Returns `RC_INDEX_NONE` if empty; optional context comparator |
| `find.h` | Linear search | Returns `UINT32_MAX` (`RC_INDEX_NONE`) if not found |
| `all_of.h` | All-match predicate test | Returns `true` if every element matches (vacuously true for empty view) |
| `any_of.h` | Any-match predicate test | Returns `true` if at least one element matches (false for empty view) |
| `none_of.h` | No-match predicate test | Returns `true` if no element matches (vacuously true for empty view) |
| `mismatch.h` | First mismatched index between two views | Returns index of first non-matching pair, or `min(n1, n2)` if overlap matches |
| `remove.h` | In-place removal | Compacts matching elements out; updates `span->num`; returns count removed |
| `rotate.h` | In-place rotation | Rotates span so element at index k moves to index 0; three-reversal algorithm |
| `transform.h` | Map/filter into array | Appends to destination, returns start index |
| `accumulate.h` | Fold view to scalar | `ACCUM_T` + `ACCUM_RESULT_T`; default func is addition |

All algorithm templates support an optional context pointer for custom
comparators/predicates.

## Test suite
Single file: `test.c` (~4,870 lines).
Covers: all containers, all algorithms, multiple element types (`int`, custom
`Record` struct), context-aware variants, edge cases.

## File inventory
```
CMakeLists.txt                  — build system (static lib + test executable)
include/richc/
  arena.h                       — arena allocator declarations + inline helpers
  platform.h                    — OS virtual memory abstraction
  debug.h                       — RC_ASSERT
  str.h                         — non-owning string view (rc_str, rc_str_pair)
  mstr.h                        — mutable/managed string (rc_mstr)
  bytes.h                       — rc_view_bytes, rc_span_bytes, rc_array_bytes (uint8_t array types)
  file.h                        — rc_load_text, rc_save_text, rc_load_binary, rc_save_binary, rc_file_error
  template_util.h               — RC_CONCAT, RC_STRINGIFY preprocessor helpers
  math/
    math.h                      — scalar utilities (deg_to_rad, min/max/sgn/gcd/clz/overflow)
    vec2i.h                     — rc_vec2i (2D integer vector; dot/wedge/lengthsqr → int64_t)
    vec3i.h                     — rc_vec3i (3D integer vector; dot/lengthsqr → int64_t; cross asserts int32_t range)
    vec2f.h                     — rc_vec2f (2D float vector)
    vec3f.h                     — rc_vec3f (3D float vector; from_vec3i)
    vec4f.h                     — rc_vec4f (4D float vector)
    aabb2f.h                    — rc_aabb2f (2D float axis-aligned bounding box)
    aabb2i.h                    — rc_aabb2i (2D integer axis-aligned bounding box)
    mat22f.h                    — rc_mat22f (2×2 column-major matrix)
    mat23f.h                    — rc_mat23f (2D affine transform)
    mat33f.h                    — rc_mat33f (3×3 column-major matrix)
    mat34f.h                    — rc_mat34f (3D affine transform, lookat)
    mat44f.h                    — rc_mat44f (4×4, ortho/perspective; det+inv in .c)
    quatf.h                     — rc_quatf (unit quaternion, Hamilton convention)
    rational.h                  — rc_rational (rational arithmetic, always canonical; trivial ops inline, rest in rational.c)
    bigint.h                    — rc_bigint (arbitrary-precision integer; sign-magnitude, arena-backed)
  template/
    hash.h                      — hash functions for built-in types, rc_str, vector types; rc_hash_combine
  template/
    array.h                     — View + Span + Array template (main container header)
    hash_map.h                  — open-addressing hash map template
    hash_set.h                  — open-addressing hash set template
    hash_trie.h                 — 16-way hash trie template
    sort.h                      — introsort template
    lower_bound.h               — lower_bound template
    upper_bound.h               — upper_bound template
    min_element.h               — index of first minimum element template
    max_element.h               — index of first maximum element template
    find.h                      — linear find template
    all_of.h                    — all-match predicate test template
    any_of.h                    — any-match predicate test template
    none_of.h                   — no-match predicate test template
    mismatch.h                  — first mismatched element between two views template
    remove.h                    — in-place removal template
    rotate.h                    — in-place rotation template
    transform.h                 — map/filter template
    accumulate.h                — fold/reduce template
src/
  arena.c                       — arena allocator implementation
  str.c                         — rc_str non-trivial function implementations
  mstr.c                        — rc_mstr function implementations
  file.c                        — rc_load_text, rc_save_text, rc_load_binary, rc_load_binary_array, rc_save_binary
  math/
    math/
      mat44f.c                  — rc_mat44f_determinant, rc_mat44f_inverse
      rational.c                — rc_rational non-trivial operations (make, from_double, int_mul, mul, int_div, div, add, sub)
      bigint.c                  — rc_bigint non-trivial operations (make, from_u64/i64, copy, reserve, add, sub, mul, divmod, div, mod)
test/
  test.c                        — full test suite (~7,700 lines, ~2231 assertions)
```

All library headers are included as `#include "richc/..."` (the `include/` directory
is the include root, exposed as a PUBLIC target include directory by CMake).

## Design notes: async file I/O (not yet implemented)

### Agreed approach
- **Arena strategy**: allocate the result buffer (and the future object) on the
  calling thread before dispatching. The background thread only writes into the
  pre-allocated buffer and completes the future — it never touches the arena.
  This sidesteps arena thread-safety entirely for now. A thread-safe arena (via a
  mutex allocated as the first item in the arena) is a separate future idea.
- **Sync primitives**: use C23 `<threads.h>` (`mtx_t`, `cnd_t`) — no platform-
  specific code needed.
- **Thread pool**: a simple `rc_thread_pool` with a fixed number of worker threads;
  the async file functions take a `rc_thread_pool *` parameter.

### Typed future template (`template/future.h`)
Follows the same define-then-include pattern as `array.h`.

Control macros:
```c
#define FUTURE_T    rc_load_text_result   /* required: the result type */
#define FUTURE_NAME rc_future_load_text   /* optional: default rc_future_FUTURE_T */
#include "richc/template/future.h"
```

Generated types:
```c
/* Arena-allocated object — never moved after creation. */
typedef struct {
    FUTURE_T value;
    mtx_t    mtx;
    cnd_t    cnd;
    bool     ready;
} rc_future_load_text_obj;

/* By-value handle — cheap to copy and pass around. */
typedef struct {
    rc_future_load_text_obj *ptr;
} rc_future_load_text;
```

Generated functions:
```c
rc_future_load_text rc_future_load_text_make(rc_arena *a);
void                rc_future_load_text_complete(rc_future_load_text f,
                                                  rc_load_text_result value);
rc_load_text_result rc_future_load_text_wait(rc_future_load_text f);
bool                rc_future_load_text_poll(rc_future_load_text f);
void                rc_future_load_text_destroy(rc_future_load_text f);
```

`complete` is called by the producer thread; `wait`/`poll` by the consumer.
`destroy` calls `mtx_destroy`/`cnd_destroy` — callers should call it when done,
but omitting it (relying on arena teardown) is harmless on all real targets.

### Async load sequence
```
main thread:
  1. open file, fseek/ftell → size
  2. rc_arena_alloc(a, size + 1)      ← buffer in caller's arena
  3. rc_future_load_text_make(a)      ← future object in caller's arena
  4. allocate work-item struct in arena, store { FILE*, buf, size, future }
  5. rc_thread_pool_submit(pool, work_fn, work_item)
  6. return future handle

background thread (work_fn):
  7. fread into pre-allocated buffer
  8. buf[size] = '\0'
  9. rc_future_load_text_complete(future, { rc_str{buf, size}, RC_FILE_OK })
     — or complete with error on fread failure (buffer remains in arena, wasted
        until arena reset, which is acceptable)
```

### Thread pool design notes (not yet implemented)

#### Singleton vs. instantiation
The async file API already takes `rc_thread_pool *` as an explicit parameter
(see public async API sketch below), so the pool must be an instantiatable
type even if only one instance is ever created in practice.  This is
consistent with the library's zero-global-state design: the caller creates
one pool in `main` and passes it where needed, exactly like `rc_arena`.

Revisit when a concrete use-case exists that would benefit from a pool —
that will clarify the right API ergonomics.

#### Queue algorithm (open question)
Three candidates, in ascending complexity:
- **Mutex + ring buffer**: one mutex, two condition variables (`not_empty`,
  `not_full`), fixed circular buffer of `{fn, arg}` pairs.  Easy to reason
  about; every submit/consume acquires the mutex.
- **Lock-free MPMC ring buffer** (Dmitry Vyukov design): each slot carries
  an `_Atomic uint32_t` sequence counter; producers and consumers CAS-claim
  slots with no mutex on the hot path.  ~50 lines; excellent in practice.
- **Chase-Lev work-stealing deque** (one per thread): local push/pop is LIFO
  and contention-free; idle threads steal FIFO from others.  Best for
  irregular recursive task graphs; probably overkill for I/O workloads.

Lean towards the MPMC ring buffer if performance matters; mutex + ring buffer
if simplicity is paramount.

#### Task memory (open question)
Options, from simplest to most flexible:
- **Embedded ring buffer**: pool struct contains an array of `{fn, arg}` pairs
  allocated at `make` time.  Submit copies fn and arg by value; caller owns
  the arg lifetime.  No per-task allocation.
- **Intrusive task struct**: caller arena-allocates a `rc_pool_task` (which
  embeds `void (*fn)(rc_pool_task *)` and `rc_pool_task *next`), then submits
  the pointer.  Pool never allocates; task payload follows the struct.
  Pattern mirrors Linux `work_struct`.
- **Pool-private arena + free-list**: pool has its own arena for task nodes;
  free-list recycles them.  Adds complexity without clear benefit over the
  intrusive approach.

#### Submit-when-full behaviour (open question)
- Block until a slot is free (safe default, can cause deadlock if workers
  submit work themselves).
- Assert/panic (caller is responsible for not overfilling; simpler).

#### Shutdown
`destroy` should drain (finish all queued tasks) then join threads.
Abandon-on-shutdown requires tasks to cooperate via a cancellation flag and
is deferred.

#### Sketch API
```c
typedef struct rc_thread_pool rc_thread_pool;   /* opaque */

rc_thread_pool *rc_thread_pool_make(uint32_t num_threads, uint32_t queue_cap,
                                     rc_arena *a);
void            rc_thread_pool_destroy(rc_thread_pool *p);  /* drain + join */
void            rc_thread_pool_submit(rc_thread_pool *p,
                                       void (*fn)(void *), void *arg);
void            rc_thread_pool_wait_all(rc_thread_pool *p); /* barrier */
```

### Public async API (sketch)
```c
rc_future_load_text  rc_load_text_async (const char *filename, rc_arena *a,
                                          rc_thread_pool *pool);
rc_future_load_text  rc_load_binary_async(const char *filename, rc_arena *a,
                                           rc_thread_pool *pool);
/* save variants don't need arena allocation on the main thread */
rc_future_file_error rc_save_text_async (const char *filename, rc_str text,
                                          rc_thread_pool *pool);
rc_future_file_error rc_save_binary_async(const char *filename,
                                           rc_view_bytes data,
                                           rc_thread_pool *pool);
```
