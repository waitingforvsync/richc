# richc

A C17 library of generic data structures, algorithms, math types, and utilities. Pure C, no external dependencies. Designed for performance: arena allocation, cache-friendly layouts, and compile-time code generation via the C preprocessor.

## Core concepts

### Arena allocation

The library never calls `malloc`, `realloc`, or `free`. All dynamic memory goes through `rc_arena` — a virtual-memory-backed bump allocator that reserves 256 MB of address space upfront and commits pages on demand.

```c
rc_arena arena = rc_arena_make_default();

// Allocate from the arena — no free needed
rc_array_int arr = rc_array_int_make(0, &arena);
rc_array_int_push(&arr, 42, &arena);

rc_arena_destroy(&arena);  // releases everything at once
```

Functions that may allocate accept `rc_arena *` as their last parameter. Functions that need private scratch space (e.g. `rc_bigint_mul`) additionally accept an `rc_arena` **by value** as a further parameter — it is consumed locally and discarded on return, leaving the caller's arena untouched.

```c
// scratch is passed by value: rc_bigint_mul uses it internally and discards it
rc_bigint_mul(&result, &b, &c, &arena, scratch);
```

### Template pattern

Generic containers and algorithms are preprocessor templates. Define a control macro, then `#include` the header. The header generates type-specific code and `#undef`s its macros so the same header can be included again for a different type.

```c
#define ARRAY_T int
#include "richc/template/array.h"
// produces: rc_view_int, rc_span_int, rc_array_int, rc_array_int_push(), ...

#define ARRAY_T float
#include "richc/template/array.h"
// produces: rc_view_float, rc_span_float, rc_array_float, rc_array_float_push(), ...
```

Custom type names can be supplied via optional macros (`ARRAY_NAME`, `ARRAY_VIEW`, `ARRAY_SPAN`) before the include.

## What's included

### Data structures

| Header | Type | Description |
|--------|------|-------------|
| `arena.h` | `rc_arena` | Virtual-memory bump allocator |
| `str.h` | `rc_str` | Non-owning string view `{const char *data; uint32_t len}` |
| `mstr.h` | `rc_mstr` | Arena-backed growable string; always null-terminated |
| `template/array.h` | `rc_array_T` | Growable arena-backed array; also generates `rc_view_T` and `rc_span_T` |
| `template/hash_map.h` | `rc_map_T` | Open-addressing hash map with tombstone deletion; SoA layout |
| `template/hash_set.h` | `rc_set_T` | Open-addressing hash set with tombstone deletion; SoA layout |
| `template/hash_trie.h` | `rc_trie_T` | 16-way radix trie over hash values; arena-backed node pool |
| `bytes.h` | `rc_view/span/array_bytes` | `uint8_t` array family |

### Hashing

`hash.h` provides `static inline uint32_t` hash functions for all common richc types, suitable for use directly as `MAP_HASH` / `SET_HASH` expressions in the hash_map and hash_set templates.

| Function | Input |
|----------|-------|
| `rc_hash_u32` / `rc_hash_i32` | `uint32_t` / `int32_t` |
| `rc_hash_u64` / `rc_hash_i64` | `uint64_t` / `int64_t` |
| `rc_hash_f32` / `rc_hash_f64` | `float` / `double` (−0 and +0 hash identically) |
| `rc_hash_ptr` | `const void *` |
| `rc_hash_bytes` | `const void *, uint32_t len` |
| `rc_hash_str` | `rc_str` (NULL/invalid safe) |
| `rc_hash_vec2i` / `rc_hash_vec3i` | `rc_vec2i` / `rc_vec3i` |
| `rc_hash_vec2f` / `rc_hash_vec3f` / `rc_hash_vec4f` | `rc_vec2f` / `rc_vec3f` / `rc_vec4f` |
| `rc_hash_rational` | `rc_rational` (always canonical, so field combination is correct) |
| `rc_hash_combine(seed, hash)` | Mix a hash into a running seed (Boost formula) |

```c
// Hashing a struct field by field
uint32_t h = rc_hash_i32(point.x);
h = rc_hash_combine(h, rc_hash_i32(point.y));

// Using with hash_map
#define MAP_KEY_T   rc_str
#define MAP_HASH(k) rc_hash_str(k)
#define MAP_EQUAL(a, b) rc_str_is_equal(a, b)
#include "richc/template/hash_map.h"
```

### Algorithms

All algorithm templates support an optional context pointer for custom comparators and predicates.

| Header | Description |
|--------|-------------|
| `template/sort.h` | Introsort (quicksort + heapsort fallback + insertion sort for n ≤ 16) |
| `template/lower_bound.h` | First index where `arr[i] >= key` |
| `template/upper_bound.h` | First index where `arr[i] > key` |
| `template/min_element.h` | Index of first minimum element; returns `RC_INDEX_NONE` if empty |
| `template/max_element.h` | Index of first maximum element; returns `RC_INDEX_NONE` if empty |
| `template/find.h` | Linear search; returns `RC_INDEX_NONE` if not found |
| `template/all_of.h` | Returns `true` if all elements satisfy the predicate (vacuously true for empty) |
| `template/any_of.h` | Returns `true` if any element satisfies the predicate (false for empty) |
| `template/none_of.h` | Returns `true` if no element satisfies the predicate (vacuously true for empty) |
| `template/mismatch.h` | Index of first mismatched element between two views; returns `min(n1, n2)` if overlap matches |
| `template/remove.h` | In-place removal by predicate; compacts span, returns count removed |
| `template/rotate.h` | In-place rotation; element at index k moves to index 0 |
| `template/transform.h` | Map/filter into an array; appends to destination |
| `template/accumulate.h` | Fold a view to a scalar |

### Math

All math headers are under `richc/math/`. All types use the `rc_` prefix with no `_t` suffix. Inline functions carry an explicit type suffix to avoid Windows macro conflicts (`near`/`far`/`min`/`max`).

| Header | Type | Notes |
|--------|------|-------|
| `math/math.h` | — | `rc_min/max/sgn/gcd/clz`, overflow checks |
| `math/vec2i.h` | `rc_vec2i` | `{int32_t x, y}`; `dot`/`wedge`/`lengthsqr` → `int64_t` |
| `math/vec3i.h` | `rc_vec3i` | `{int32_t x, y, z}`; `cross` asserts `int32_t` range |
| `math/vec2f.h` | `rc_vec2f` | 2D float vector |
| `math/vec3f.h` | `rc_vec3f` | 3D float vector |
| `math/vec4f.h` | `rc_vec4f` | 4D float vector |
| `math/aabb2f.h` | `rc_aabb2f` | 2D float AABB |
| `math/aabb2i.h` | `rc_aabb2i` | 2D integer AABB |
| `math/mat22f.h` | `rc_mat22f` | 2×2 column-major float matrix |
| `math/mat23f.h` | `rc_mat23f` | 2D affine transform `{rc_mat22f m; rc_vec2f t}` |
| `math/mat33f.h` | `rc_mat33f` | 3×3 column-major float matrix |
| `math/mat34f.h` | `rc_mat34f` | 3D affine transform; includes `make_lookat` |
| `math/mat44f.h` | `rc_mat44f` | 4×4 float matrix; ortho/perspective; determinant/inverse |
| `math/quatf.h` | `rc_quatf` | Unit quaternion; Hamilton convention |
| `math/rational.h` | `rc_rational` | Exact rational arithmetic `{int64_t num, denom}`; always canonical |
| `math/bigint.h` | `rc_bigint` | Arbitrary-precision integer; sign-magnitude, arena-backed |

Matrix storage is column-major throughout. `rc_mat34f` and `rc_mat23f` represent affine transforms as `m * v + t`.

`rc_rational` is always in lowest terms with positive denominator. Arithmetic uses GCD pre-reduction to keep intermediate values small.

`rc_bigint` uses 32-bit limbs with 64-bit intermediates. All binary operations use a 3-address form (`result`, `b`, `c`) and handle aliasing uniformly by capturing input pointers before any reallocation. Division is via Knuth Algorithm D (base 2³²). `int64_t` shortcut variants (`rc_bigint_int_add` etc.) build a transient stack-backed bigint and delegate, so no extra arena allocation is needed for the operand.

```c
rc_arena arena   = rc_arena_make_default();
rc_arena scratch = rc_arena_make_default();

rc_bigint a = rc_bigint_from_i64(1000000000LL * 1000000000LL, &arena);
rc_bigint b = rc_bigint_from_i64(999999937, &arena);  // a prime
rc_bigint q = rc_bigint_make(0, &arena);
rc_bigint r = rc_bigint_make(0, &arena);

rc_bigint_divmod(&q, &r, &a, &b, &arena, scratch);

// or using the int64_t shortcut:
rc_bigint_int_add(&a, &a, 1, &arena);  // a += 1

rc_arena_destroy(&scratch);
rc_arena_destroy(&arena);
```

### File I/O

```c
rc_arena arena = rc_arena_make_default();

// Text
rc_load_text_result t = rc_load_text("data.txt", &arena);
if (t.error == RC_FILE_OK)
    process(t.text);  // rc_str backed by arena memory, always null-terminated

rc_save_text("out.txt", RC_STR("hello"));

// Binary
rc_view_bytes data = rc_load_binary("data.bin", &arena).data;

rc_arena_destroy(&arena);
```

## Usage

Add `include/` to your include path and link against the library. All headers are included as `"richc/..."`.

```c
#include "richc/arena.h"
#include "richc/str.h"
#include "richc/math/vec3f.h"

#define ARRAY_T int
#include "richc/template/array.h"

#define SORT_T int
#include "richc/template/sort.h"
```

## Building

Requires **clang** and **CMake + Ninja**. The build compiles as C17.

```sh
cmake -B build -G Ninja -DCMAKE_C_COMPILER=clang
cmake --build build
```

Tests run automatically as a post-build step. A failed assertion triggers a debug break and the build reports an error.

## Naming conventions

- All `snake_case`. Library prefix `rc_` on all public types and functions; `RC_` on macros.
- Template control macros (`ARRAY_T`, `SORT_CMP`, etc.) are unprefixed ALL_CAPS — they are consumed at include-time and leave no compiled trace.
- Types returned or constructed by value use `rc_<type>_make(...)` or `rc_<type>_from_<other>(...)`. Non-owning reinterpretations use `rc_<type>_as_<other>(...)`.
- `uint32_t` is used as the index type throughout; `RC_INDEX_NONE` (`UINT32_MAX`) is the sentinel for "not found".
