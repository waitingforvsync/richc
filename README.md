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

- **String view** (`richc/str.h`) - `rc_str`, a non-owning pointer-and-length
  view with comparison, slicing, searching, trimming, splitting, and conversion
  to a C string. Never allocates.
- **View / span / array** (`richc/template/array.h`) - the template above. A
  read-only `rc_view`, a mutable `rc_span`, and a growable arena-backed
  `rc_array`, sharing an anonymous union so conversions between them are
  typesafe field accesses. Arrays grow geometrically; spans and views are
  non-owning windows.
- **Byte buffers** (`richc/bytes.h`) - `rc_view_bytes` / `rc_span_bytes` /
  `rc_array_bytes`, the array template instantiated for `uint8_t`.
- **File I/O** (`richc/file.h`) - whole-file load and save with `rc_str`
  filenames; immutable and mutable load variants returning `rc_str`/`rc_mstr`
  and `rc_view_bytes`/`rc_array_bytes`.
- **Scalar ops** (`richc/ops.h`) - bit reinterpretation, integer min/max/sign,
  GCD, count-leading-zeros, and overflow checks.
- **Math** (`richc/math/`) - integer vectors `rc_vec2i` / `rc_vec3i` (arithmetic,
  dot/wedge/cross, etc.); more vector, matrix, and quaternion types to follow.
- **Hashing** (`richc/hash.h`) - `uint32_t` hashers for integers, floats,
  pointers, byte sequences, and strings, plus `rc_hash_combine`.
- **Unit-test framework** (`richc/test.h`) - tests that register themselves
  automatically via linker sections, typed `RC_CHECK` assertions, group
  fixtures, and a filtering test runner. Part of the core library; it costs
  nothing in builds that do not reference it.

## Documentation

See [docs/richc.md](docs/richc.md) for detailed reference documentation of every
public type, macro, and function, organised by header.

## License

MIT. See [LICENSE](LICENSE).
