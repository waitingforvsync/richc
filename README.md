# richc

richc is a C17 helper library designed to replace the C standard library, and
offering an assortment of additional functionality. It favours high performance:
cache-friendly data layouts, arena allocation, and compile-time code generation
through the C preprocessor.

The library is organised in two layers within this one repository:

- **core** - generic data structures, algorithms, math types, string handling,
  file I/O, and a built-in unit-test framework. Pure C, no external
  dependencies.
- **app** - windowed application scaffolding, input handling, an OpenGL 3.3
  graphics abstraction, and image/font loading. Built on top of core; uses
  GLFW, glad, and miniz as private implementation details that never appear in
  the public API.

## Building

Requires clang and CMake with Ninja. The library compiles as C17.

```sh
cmake -B build -G Ninja -DCMAKE_C_COMPILER=clang
cmake --build build
```

Use a separate build directory per compiler; do not mix compilers in one build
tree. The core test executable runs automatically as a post-build step, so a
failing test makes the build report an error.

## Using the library

Add `core/include` to your include path and link against the `richc` target.
All headers are included under the `richc/` namespace.

```c
#include "richc/str.h"

void example(void)
{
    rc_str s = RC_STR("file/path/to");
    rc_str_pair p = rc_str_last_split(s, RC_STR("/"));
    // p.first == "file/path", p.second == "to"
}
```

If you build with CMake, link the target directly:

```cmake
add_subdirectory(richc)
target_link_libraries(your_app PRIVATE richc)
```

## Template headers

Generic containers are preprocessor templates. A header under `richc/template/`
generates type-specific code for an element type you choose: define the control
macro(s), then include the header. The header `#undef`s its macros at the end,
so you can include it again for another type.

```c
#define RC_ARRAY_TYPE int
#include "richc/template/array.h"
// now: rc_view_int, rc_span_int, rc_array_int and their operations

#define RC_ARRAY_TYPE float
#define RC_ARRAY_NAME float
#include "richc/template/array.h"
// now: rc_view_float, rc_span_float, rc_array_float
```

The generated types are always named `rc_array_<suffix>`, `rc_span_<suffix>`,
and `rc_view_<suffix>`. The optional `RC_ARRAY_NAME` gives the `<suffix>`
(defaulting to the element type's spelling, so it must be a single identifier -
supply a name for multi-token types such as `unsigned char`).

## What it contains

Available now in core:

- **Arena allocator** (`richc/arena.h`) - `rc_arena`, a virtual-memory-backed
  bump allocator that reserves a large address range up front and commits pages
  on demand. It is the library's only allocation primitive; every allocating
  operation takes an `rc_arena *`. Allocation never returns NULL (running out of
  space panics), and the latest allocation grows in place. Passing an arena by
  value gives a scratch snapshot whose allocations are discarded on return.
- **String view** (`richc/str.h`) - `rc_str`, a non-owning pointer-and-length
  view with comparison, slicing, searching, trimming, splitting, and conversion
  to a C string. Never allocates.
- **Mutable string** (`richc/mstr.h`) - `rc_mstr`, an arena-backed growable
  string that shares its layout with `rc_str` (its current contents are always a
  valid `rc_str` view) and keeps a trailing null terminator. Append, append-char,
  replace, reserve, reset, and deinit.
- **View / span / array** (`richc/template/array.h`) - the template above. A
  read-only `rc_view`, a mutable `rc_span`, and a growable arena-backed
  `rc_array`, sharing an anonymous union so conversions between them are
  typesafe field accesses. Arrays grow geometrically; spans and views are
  non-owning windows. Spans also include in-place `reverse` and `rotate`
  (Gries-Mills block swap).
- **Byte buffers** (`richc/bytes.h`) - `rc_view_bytes` / `rc_span_bytes` /
  `rc_array_bytes`, the array template instantiated for `uint8_t`.
- **Bit array** (`richc/bitset.h`) - `rc_bitset`, a growable arena-backed array
  of bits packed into `uint32_t` words, with set/clear/test, geometric `push`,
  and whole-word set-bit iteration. `bitset_foreach.h` adds an iterator template
  that calls a macro on each set bit, with an optional context.
- **Hashing** (`richc/hash.h`) - `static inline` 32-bit hash functions for the
  fixed-width integers, floats, pointers, byte ranges, `rc_str`, and the vector
  and rational types, plus `hash_combine`. Usable directly as the hash expression
  for the hash-table templates.
- **Hash trie** (`richc/template/hash_trie.h`) - a template for a 16-way
  arena-backed map or set keyed by a 64-bit hash; nodes are pooled 16 to a block
  in an internal `rc_pool` (so blocks emptied by delete are recycled), and one
  pool can back many independent tries.
- **Object pool** (`richc/template/pool.h`) - a template for an index-stable
  free-list pool over an `rc_array`: `alloc` returns a stable index, `free`
  recycles it, and freed slots are reused. `pool_foreach.h` adds an iterator
  template that visits the live entries (via a scratch-arena bitset of the dead
  slots), calling a macro on each with an optional context.
- **Sort** (`richc/template/sort.h`) - a template generating an in-place
  introsort over a mutable `rc_span` (quicksort with a heapsort fallback and
  insertion sort for small spans), with an optional custom comparator and
  context pointer.
- **Binary search** (`richc/template/lower_bound.h`,
  `richc/template/upper_bound.h`) - templates generating `lower_bound` and
  `upper_bound` over a sorted `rc_view`: the first index whose element is
  `>= value` and the first whose element is `> value`, with an optional custom
  comparator and context pointer.
- **Min / max element** (`richc/template/min_element.h`,
  `richc/template/max_element.h`) - templates returning the index of the leftmost
  minimum or maximum of an `rc_view` (`RC_INDEX_NONE` if empty), with an optional
  custom comparator and context pointer.
- **File I/O** (`richc/file.h`) - whole-file load and save with `rc_str`
  filenames; immutable and mutable load variants returning `rc_str`/`rc_mstr`
  and `rc_view_bytes`/`rc_array_bytes`.
- **Scalar ops** (`richc/ops.h`) - bit reinterpretation, integer min/max/sign,
  GCD, count-leading-zeros, count-trailing-zeros, and overflow checks.
- **Math** (`richc/math/`) - integer vectors `rc_vec2i` / `rc_vec3i` (arithmetic,
  dot/wedge/cross, etc.), float vectors `rc_vec2f` / `rc_vec3f` / `rc_vec4f`
  (the same plus length/normalize/lerp), axis-aligned boxes `rc_box2i` /
  `rc_box2f`, and column-major float matrices `rc_mat22f` / `rc_mat33f` /
  `rc_mat44f` with the 2D/3D affine transforms `rc_mat23f` / `rc_mat34f`
  (multiply, determinant, inverse, rotation/projection/look-at builders), and
  the quaternion `rc_quatf` (compose/transform, slerp, exp/log/pow, and
  matrix conversion both ways), exact rationals `rc_rational` (canonical form,
  overflow-checked arithmetic, overflow-safe comparison), plus analytic
  quadratic and cubic root solvers.
- **Unit-test framework** (`richc/test.h`) - a small assertion-based test runner
  (`RC_TEST`, `RC_TEST_STEP`, group fixtures, and `RC_CHECK` with type-aware
  comparison) used by the core test suite and available for your own tests.
- **Macros and assertions** (`richc/macros.h`) - `RC_ASSERT` (debug-only, traps
  on failure) and `RC_PANIC` (always active), plus `RC_CONCAT`, `RC_STRINGIFY`,
  and the `RC_INDEX_NONE` sentinel.
- **Hashing** (`richc/hash.h`) - `uint32_t` hashers for integers, floats,
  pointers, byte sequences, strings, and the vector types, plus
  `rc_hash_combine`.
- **Unit-test framework** (`richc/test.h`) - tests that register themselves
  automatically via linker sections, typed `RC_CHECK` assertions, group
  fixtures, and a filtering test runner. Part of the core library; it costs
  nothing in builds that do not reference it.

## Documentation

See [docs/richc.md](docs/richc.md) for detailed reference documentation of every
public type, macro, and function, organised by header.

## License

MIT. See [LICENSE](LICENSE).
