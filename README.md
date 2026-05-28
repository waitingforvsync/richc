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

## What it contains

Available now in core:

- **String view** (`richc/str.h`) - `rc_str`, a non-owning pointer-and-length
  view with comparison, slicing, searching, trimming, splitting, and conversion
  to a C string. Never allocates.
- **Unit-test framework** (`richc/test.h`) - tests that register themselves
  automatically via linker sections, typed `RC_CHECK` assertions, group
  fixtures, and a filtering test runner. Part of the core library; it costs
  nothing in builds that do not reference it.

## Documentation

See [docs/richc.md](docs/richc.md) for detailed reference documentation of every
public type, macro, and function, organised by header.

## License

MIT. See [LICENSE](LICENSE).
