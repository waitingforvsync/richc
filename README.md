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

## Why use richc?

C has a reputation as a language past its time - unfit for modern engineering,
kept alive by legacy. richc is a counter-argument: with a small set of
conventions applied consistently, C code can be elegant, readable, safe(ish),
and very fast, with no runtime, no garbage collector, and no language machinery
working against you.

- **An allocation model that is explicit but simple.** One allocator - the
  arena - and every allocating call names it. No malloc/free bookkeeping, no
  NULL-checking boilerplate (allocation never returns NULL), no hidden heap
  traffic: where memory comes from is visible at every call site.
- **Arenas are the arbiters of lifetime.** Lifetimes are reasoned about in
  broad strokes - per frame, per load, per session - rather than per object.
  Freeing is wholesale, so whole classes of leaks and double-frees have
  nowhere to occur, and temporary memory is a by-value arena that cleans
  itself up on return.
- **Containers are views over storage, not owners of storage.** An array
  borrows its arena's memory; spans and views borrow the array's. Nothing has
  a destructor, conversions are field accesses, and passing data around never
  raises the question of who frees it - nobody does; the arena owns it.
- **Identity is an index or a handle, not an address.** Indices survive
  growth and relocation, are half the size of a pointer, and serialise
  trivially. Generational handles go further: a stale reference is *detected*
  and traps, instead of silently aliasing whatever lives there now.
- **Objects are context-free.** An object holds only what is intrinsic to
  itself; operations take the context - the arena to grow into, the pool a
  trie lives in. Objects stay small, trivially copyable, and free of
  back-pointers.
- **Zero is a valid value.** A zero-initialised trie, pool, bitset, lock, or
  handle is ready to use, and zero fields in a descriptor mean "the default".
  Construction is `= {0}`, and a whole class of initialisation bugs never
  exists.
- **Data is just data.** A struct is bytes: memcpy it, zero it, write it to
  disk, inspect it in a debugger and see the truth. No vtables, no
  destructors, no operator overloads, no hidden copies - unlike C++ objects,
  data carries no logic, so the only code that runs is the code you can see
  at the call site, and the cost of a line is what it looks like.
- **Safe(ish), by convention plus cheap checks.** Bounds-checked accessors,
  asserted preconditions, untrusted input that returns errors rather than
  trapping, handles that catch use-after-free. Not a borrow checker - but the
  classic C footguns are each dealt with at the point where they arise.
- **Performance falls out of the design.** Contiguous, cache-friendly
  layouts; bump allocation; growth in place over a reserved address range so
  pointers stay put; containers monomorphised per type, as optimisable as
  hand-written code. Fast is the default, not an optimisation pass.
- **Builds are fast and stay fast.** Full rebuilds in seconds: no template
  instantiation explosions, no dependency graph to appease. Plain C17 with
  zero dependencies in core compiles everywhere today, will still compile
  decades from now, and is trivially callable from any language with a C FFI.

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

Generic containers and algorithms are preprocessor templates: define a control
macro, include a `richc/template/` header, and it generates fully typed code,
then `#undef`s its macros so it can be included again for another type. No
runtime polymorphism, no `void *`.

```c
#define RC_ARRAY_TYPE int
#include "richc/template/array.h"
// now: rc_view_int, rc_span_int, rc_array_int and their operations
```

## Conventions

- Everything is `snake_case`. Public types and functions are prefixed `rc_`,
  public macros `RC_`.
- Functions that may allocate take an `rc_arena *`, named `arena`, as their
  *last* parameter; it may be NULL when the call provably needs no allocation.
  An `rc_arena` passed *by value* is private scratch, reclaimed for free on
  return.
- Indices are always `uint32_t`, with `RC_INDEX_NONE` as the "not found" /
  "invalid" sentinel.
- Results are returned by value - often bundled into a small struct - rather
  than written through out-parameters, which are reserved for when the
  alternative is genuinely worse (filling a caller-owned buffer, or a large
  object that should not be copied).
- Preconditions trap; errors are for what a caller can handle. Programmer
  errors (a NULL pointer, an out-of-range index) are guarded by asserts;
  returned error results are reserved for conditions a caller genuinely
  handles, like a missing file or malformed external data.
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
