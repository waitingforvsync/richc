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
- **Language standard: C23** — compile with `-std=c23`
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
  `RC_DEBUG_BREAK_`, `RC_ARRAY_ONCE_`, `RC_MAP_ONCE_`, `RC_TRIE_ONCE_`.
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
| `math/math.h` | — | `rc_deg_to_rad`, `rc_min/max/sgn_i32/i64`, `rc_gcd_i32/i64`, `rc_clz_u32/u64`, `rc_mul/add_overflows_u64` |
| `math/vec2i.h` | `rc_vec2i` | 2D integer vector; `{int32_t x, y}` |
| `math/vec2f.h` | `rc_vec2f` | 2D float vector; also emits `rc_view/span/array_vec2f` |
| `math/vec3f.h` | `rc_vec3f` | 3D float vector; also emits `rc_view/span/array_vec3f` |
| `math/vec4f.h` | `rc_vec4f` | 4D float vector; also emits `rc_view/span/array_vec4f` |
| `math/aabb2f.h` | `rc_aabb2f` | 2D axis-aligned bounding box `{rc_vec2f min, max}` |
| `math/mat22f.h` | `rc_mat22f` | 2×2 column-major float matrix; `{rc_vec2f cx, cy}` |
| `math/mat23f.h` | `rc_mat23f` | 2D affine transform; `{rc_mat22f m; rc_vec2f t}` |
| `math/mat33f.h` | `rc_mat33f` | 3×3 column-major float matrix; `{rc_vec3f cx, cy, cz}` |
| `math/mat34f.h` | `rc_mat34f` | 3D affine transform; `{rc_mat33f m; rc_vec3f t}`; includes `make_lookat` |
| `math/mat44f.h` | `rc_mat44f` | 4×4 column-major float matrix; `determinant`/`inverse` in `.c` |
| `math/quatf.h` | `rc_quatf` | Unit quaternion; `{rc_vec3f xyz; float w}`; Hamilton convention |
| `math/rational.h` | `rc_rational` | Rational arithmetic; `{int64_t num, denom}`; always canonical |

Matrix storage is column-major throughout: `cx` is the first column, etc.
The `make_transpose(rx, ry, rz, ...)` constructors accept *row* vectors and store
them transposed — useful for specifying a matrix row-by-row.

`rc_mat34f` and `rc_mat23f` represent affine transforms applied as `m * v + t`.
`rc_mat34f_inverse` and `rc_mat23f_inverse` assume the linear part is invertible.

`rc_quatf_make_angle_axis` normalises the axis internally.
`rc_mat33f_from_quatf` and `rc_quatf_from_mat33f` form a roundtrip for any rotation.
`rc_quatf_vec3f_transform` uses the Rodrigues formula (no matrix allocation needed).

`rc_rational` is always in canonical form: `denom > 0`, `gcd(|num|, denom) == 1`.
`rc_rational_make(n, 0)` → invalid `{0, 0}`.  All arithmetic routes through
`rc_rational_make` so the invariant is preserved.

### Hash map
Header: `hash_map.h`
Open-addressing with linear probing, Structure-of-Arrays layout.
Default type name: `rc_map_T` (keyed on `MAP_KEY_T`).
Tombstone deletion. Load factor < 3/4; capacity always power-of-two.
Zero-initializable empty state.
Operations: `add`, `remove`, `find`, `contains`, `reserve`.

### Hash trie
Header: `hash_trie.h`
16-way (4 bits per level) radix trie over hash values.
Default type name: `rc_trie_T` (keyed on `TRIE_KEY_T`);
pool type `rc_trie_T_pool`, node type `rc_trie_T_node`.
Flat arena-backed node pool; multiple tries can share one pool.
Operations: `create`, `find`, `add`, `delete`.

## File I/O

Headers: `file.h` (includes `bytes.h`) / Implementation: `src/file.c`.

```c
rc_file_error              { RC_FILE_OK=0, RC_FILE_ERROR_NOT_FOUND,
                             RC_FILE_ERROR_ACCESS_DENIED, RC_FILE_ERROR_TOO_LARGE,
                             RC_FILE_ERROR_IO }
rc_load_text_result        { rc_str text; rc_file_error error; }
rc_load_binary_result      { rc_view_bytes data; rc_file_error error; }
rc_load_binary_array_result{ rc_bytes data; rc_file_error error; }
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
  `rc_bytes` with `cap` set to the file size. Use `rc_array_uint8_t_push` etc.
  to mutate after loading. Returns `{0}` on failure.
- `rc_save_binary(filename, data)` — writes `rc_view_bytes` to file (create or
  truncate). Returns `rc_file_error`.

### bytes.h
Convenience include-guarded header that generates the `uint8_t` array family:
```c
rc_view_bytes  { const uint8_t *data; uint32_t num; }
rc_span_bytes  {       uint8_t *data; uint32_t num; }
rc_bytes       {       uint8_t *data; uint32_t num; uint32_t cap; }
```
Operation functions are named after `ARRAY_T` (not `ARRAY_NAME`):
`rc_array_uint8_t_push`, `rc_array_uint8_t_push_n`, etc.

## Algorithms

| Header | Function | Notes |
|--------|----------|-------|
| `sort.h` | Introsort | Quicksort + heapsort fallback + insertion sort (n≤16) |
| `lower_bound.h` | First index where `arr[i] >= key` | Matches C++ `std::lower_bound` |
| `upper_bound.h` | First index where `arr[i] > key` | Matches C++ `std::upper_bound` |
| `find.h` | Linear search | Returns `UINT32_MAX` (`RC_INDEX_NONE`) if not found |
| `transform.h` | Map/filter into array | Appends to destination, returns start index |
| `accumulate.h` | Fold view to scalar | `ACCUM_T` + `ACCUM_RESULT_T`; default func is addition |

All algorithm templates support an optional context pointer for custom
comparators/predicates.

## Test suite
Single file: `test.c` (~1,900 lines).
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
  bytes.h                       — rc_view_bytes, rc_span_bytes, rc_bytes (uint8_t array types)
  file.h                        — rc_load_text, rc_save_text, rc_load_binary, rc_save_binary, rc_file_error
  template_util.h               — RC_CONCAT, RC_STRINGIFY preprocessor helpers
  math/
    math.h                      — scalar utilities (deg_to_rad, min/max/sgn/gcd/clz/overflow)
    vec2i.h                     — rc_vec2i (2D integer vector)
    vec2f.h                     — rc_vec2f (2D float vector) + array types
    vec3f.h                     — rc_vec3f (3D float vector) + array types
    vec4f.h                     — rc_vec4f (4D float vector) + array types
    aabb2f.h                    — rc_aabb2f (2D axis-aligned bounding box)
    mat22f.h                    — rc_mat22f (2×2 column-major matrix)
    mat23f.h                    — rc_mat23f (2D affine transform)
    mat33f.h                    — rc_mat33f (3×3 column-major matrix)
    mat34f.h                    — rc_mat34f (3D affine transform, lookat)
    mat44f.h                    — rc_mat44f (4×4, ortho/perspective; det+inv in .c)
    quatf.h                     — rc_quatf (unit quaternion, Hamilton convention)
    rational.h                  — rc_rational (rational arithmetic, always canonical)
  template/
    array.h                     — View + Span + Array template (main container header)
    hash_map.h                  — open-addressing hash map template
    hash_trie.h                 — 16-way hash trie template
    sort.h                      — introsort template
    lower_bound.h               — lower_bound template
    upper_bound.h               — upper_bound template
    find.h                      — linear find template
    transform.h                 — map/filter template
    accumulate.h                — fold/reduce template
src/
  arena.c                       — arena allocator implementation
  str.c                         — rc_str non-trivial function implementations
  mstr.c                        — rc_mstr function implementations
  file.c                        — rc_load_text, rc_save_text, rc_load_binary, rc_load_binary_array, rc_save_binary
  math/
    mat44f.c                    — rc_mat44f_determinant, rc_mat44f_inverse
test/
  test.c                        — full test suite (~3650 lines, 1500 assertions)
```

All library headers are included as `#include "richc/..."` (the `include/` directory
is the include root, exposed as a PUBLIC target include directory by CMake).
