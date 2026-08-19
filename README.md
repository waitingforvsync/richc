# richc

richc is a C17 helper library for making console and windowed apps easier and
more fun to write in plain C17. It favours high performance: cache-friendly data
layouts, arena allocation, and compile-time code generation through the C
preprocessor.

The library is organised in two layers within this one repository:

- **core** - generic data structures, algorithms, math types, string handling,
  file I/O, and a built-in unit-test framework. Pure C, no external
  dependencies.
- **app** - windowing, graphics API abstraction, input handling, font management, and CPU-side
  image loading, built on top of core. Uses GLFW and glad as private implementation
  details that never appear in the public API.

## Building

It's written in C17 and tested with clang, CMake and Ninja, but should build with
any major compiler on Windows, Linux or MacOS.

```sh
cmake -B build -G Ninja -DCMAKE_C_COMPILER=clang
cmake --build build
```

## Using the library

If you build with CMake, the easiest way is to add richc as a git submodule or just
vendor it into your own tree, and link the target directly:

```cmake
add_subdirectory(richc)
target_link_libraries(your_app PRIVATE richc)
# A windowed app links with richc_app; internal deps like richc and GLFW are pulled in for you
# target_link_libraries(your_app PRIVATE richc_app)
```

```c
#include "richc/str.h"

void example(void)
{
    rc_str s = RC_STR("file/path/to");
    rc_str_pair p = rc_str_last_split(s, RC_STR("/"));
    // p.first == "file/path", p.second == "to"
}
```

## Philosophy

A handful of principles run through the whole library; once they are clear,
most of the API follows from them.

- **Arenas are the only allocation.** The library never calls `malloc` or
  `free`. Every allocating function takes an `rc_arena *` - a
  virtual-memory-backed bump allocator - as its last parameter. Allocation
  never returns NULL (running out of space is a panic, not an error to handle),
  and you free an arena, not individual objects. Passing an arena *by value*
  hands the callee scratch memory that is reclaimed for free on return.
- **Objects are context-free**. An object should only contain the information
  intrinsic to itself. A string shouldn't know how it was allocated. An array
  shouldn't know where to grow itself. The caller will always pass this context.
- **Views and spans, not pointer-and-length pairs.** `rc_view` is a read-only
  `{data, num}`, `rc_span` its mutable counterpart, and `rc_array` a growable
  arena-backed array. They share an anonymous union, so converting an array to
  a span, or a span to a view, is a typesafe field access.
- **Indices, not pointers.** Indices are always `uint32_t`, with
  `RC_INDEX_NONE` as the "not found" / "invalid" sentinel. An index stays valid
  across container growth where a pointer may not, so APIs return and accept
  indices wherever that makes sense. This is both safer and uses less memory
  for storage.
- **Value semantics where possible.** Small objects are generally passed by
  value, and functions return their result - often bundled into a
  small struct - rather than writing through an out-parameter. Out-parameters
  are reserved for when the alternative is genuinely worse: filling a
  caller-owned buffer, or a large object that should not be copied.
- **Containers and algorithms are preprocessor templates.** Define a control
  macro, include a `richc/template/` header, and it generates type-specific
  code, then `#undef`s its macros so it can be included again for another type.
  No runtime polymorphism, no `void *`.

  ```c
  #define RC_ARRAY_TYPE int
  #include "richc/template/array.h"
  // now: rc_view_int, rc_span_int, rc_array_int and their operations
  ```
- **Preconditions trap; errors are for what a caller can handle.** Programmer
  errors - a NULL pointer, an out-of-range index - are guarded by asserts and
  trap in debug builds. A returned error result is reserved for conditions a
  caller is genuinely expected to handle, like a file that fails to open or
  malformed data read from disk.

## Conventions

- Everything is `snake_case`. Public types and functions are prefixed `rc_`,
  public macros `RC_`.
- Functions are named by role:
  - `rc_<type>_make(...)` / `rc_<type>_make_<thing>(...)` - constructor,
    returning a fresh `rc_<type>` by value.
  - `rc_<type>_from_<other>(...)` - construct from a value of a different type.
  - `rc_<type>_as_<other>(...)` - reinterpret as another type without
    allocating or copying; the source stays usable.
  - `rc_<type>_to_<other>(...)` - convert into another type by *moving*,
    consuming the source so only the result owns the resource.
  - `rc_<type>_is_<predicate>(...)` - returns a `bool` (e.g. `is_valid`,
    `is_empty`).
- A trailing underscore (e.g. `RC_CHECK_IMPL_`) marks an internal symbol that
  happens to be visible in a public header; it is not part of the API.

## Reference

Detailed reference documentation of every public type, macro, and function
lives in [docs/](docs/), organised by header:

- **[Core reference](docs/core.md)** - data structures, algorithms, math,
  strings, I/O.
- **[App reference](docs/app.md)** - windowing, input, image loading, packing,
  atlasing, and the gfx GPU abstraction.

For learning the gfx layer from scratch, start with the tutorial,
**[An introduction to richc gfx](docs/gfx_intro.md)** - it builds from a window
and a triangle up to offscreen rendering, MSAA, instancing, and storage
buffers, then hands over to the reference.

## Authorship / AI disclosure ##

richc was designed and mostly implemented by me, Rich Talbot-Watkins, and
grew over many years of needing C helpers and abstractions for personal projects.
This repo is the result of trying to bring everything to a single place.

AI was used increasingly over the last months to write documentation, tests, and
help implement some of the new features. Design was all mine, always guided by
the library philosophies described above.

## License

MIT. See [LICENSE](LICENSE).
