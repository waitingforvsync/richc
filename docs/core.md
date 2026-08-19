# richc core reference

Reference for the **core** layer: generic data structures, algorithms, math
types, string handling, concurrency, and file I/O, with no external
dependencies. See the [README](../README.md) for the shared philosophy and
conventions, and [app.md](app.md) for the app layer. The documentation is
grouped by category; every public type, function, and macro is covered.

## Contents

- [Arena allocator](#arena-allocator) - `richc/arena.h`
- [String view](#string-view) - `richc/str.h`
- [Mutable string](#mutable-string) - `richc/mstr.h`
- [Container templates](#container-templates) - `richc/template/`
  - [array.h - view, span, array](#richctemplatearrayh---view-span-array)
  - [hash_trie.h - 16-way hash trie](#richctemplatehash_trieh---16-way-hash-trie)
  - [pool.h - free-list object pool](#richctemplatepoolh---free-list-object-pool)
  - [genpool.h - generational object pool](#richctemplategenpoolh---generational-object-pool)
- [Generational pool handle](#generational-pool-handle) - `richc/genpool_handle.h`
- [Ready-made arrays](#ready-made-arrays) - `richc/array/`, `richc/math/array/`
- [Byte buffers](#byte-buffers) - `richc/bytes.h`
- [Bit array](#bit-array) - `richc/bitset.h`
- [Algorithm templates](#algorithm-templates) - `richc/template/algorithm/`
  - [sort.h - introsort](#richctemplatealgorithmsorth---introsort)
  - [lower_bound.h / upper_bound.h - binary search](#richctemplatealgorithmlower_boundh--upper_boundh---binary-search)
  - [min_element.h / max_element.h - extremes](#richctemplatealgorithmmin_elementh--max_elementh---extremes)
  - [bitset_foreach.h - iterate set bits](#richctemplatealgorithmbitset_foreachh---iterate-set-bits)
  - [pool_foreach.h - iterate live pool entries](#richctemplatealgorithmpool_foreachh---iterate-live-pool-entries)
  - [genpool_foreach.h - iterate live genpool entries](#richctemplatealgorithmgenpool_foreachh---iterate-live-genpool-entries)
  - [hash_trie_foreach.h - iterate every trie entry](#richctemplatealgorithmhash_trie_foreachh---iterate-every-trie-entry)
- [Hashing](#hashing) - `richc/hash.h`
- [Math](#math) - `richc/math/`
  - [vec2i.h](#richcmathvec2ih---2d-integer-vector) /
    [vec3i.h](#richcmathvec3ih---3d-integer-vector) - integer vectors
  - [vec2f.h](#richcmathvec2fh---2d-float-vector) /
    [vec3f.h](#richcmathvec3fh---3d-float-vector) /
    [vec4f.h](#richcmathvec4fh---4d-float-vector) - float vectors
  - [box2i.h](#richcmathbox2ih---2d-integer-box) /
    [box2f.h](#richcmathbox2fh---2d-float-box) - axis-aligned boxes
  - [mat22f.h](#richcmathmat22fh---2x2-float-matrix) /
    [mat23f.h](#richcmathmat23fh---2d-affine-transform) /
    [mat33f.h](#richcmathmat33fh---3x3-float-matrix) /
    [mat34f.h](#richcmathmat34fh---3d-affine-transform) /
    [mat44f.h](#richcmathmat44fh---4x4-float-matrix) - matrices
  - [quatf.h](#richcmathquatfh---quaternion) - quaternion
  - [rational.h](#richcmathrationalh---rational-arithmetic) - exact rationals
  - [solve.h](#richcmathsolveh---polynomial-root-solvers) - root solvers
- [Rectangle packing](#rectangle-packing) - `richc/rect_pack.h`
- [Random numbers](#random-numbers) - `richc/random.h`
- [Scalar operations](#scalar-operations) - `richc/ops.h`
- [File I/O](#file-io) - `richc/file.h`
- [Decompression](#decompression) - `richc/zip/inflate.h`
- [Time](#time) - `richc/time.h`
- [Concurrency](#concurrency) - `richc/thread/`
  - [atomic.h - atomics](#richcthreadatomich---atomics)
  - [spinlock.h - spinlock](#richcthreadspinlockh---spinlock)
  - [mutex.h - mutexes](#richcthreadmutexh---mutexes)
  - [rwlock.h - reader/writer lock](#richcthreadrwlockh---readerwriter-lock)
  - [cond.h - condition variable](#richcthreadcondh---condition-variable)
  - [semaphore.h - counting semaphore](#richcthreadsemaphoreh---counting-semaphore)
  - [thread.h - threads and utilities](#richcthreadthreadh---threads-and-utilities)
  - [tls.h - thread-local keys](#richcthreadtlsh---thread-local-keys)
  - [scheduler.h - task scheduler](#richcthreadschedulerh---task-scheduler)
  - [future.h - typed futures](#richctemplatefutureh---typed-futures)
- [Unit testing](#unit-testing) - `richc/test.h`
- [Macros and assertions](#macros-and-assertions) - `richc/macros.h`

---

## Arena allocator

`richc/arena.h`. `rc_arena` is the library's only allocation primitive: a
virtual-memory-backed bump allocator that reserves a large address range up
front (so the base never moves and pointers stay valid for the arena's
lifetime) and commits physical pages on demand. Allocation is a pointer bump,
aligned to `RC_MAX_ALIGN`, and never returns NULL - running out of space is an
`RC_PANIC`, so results are never NULL-checked. Lifetimes are wholesale: free an
arena, not individual objects. Passing an arena *by value* hands the callee a
snapshot of the bump pointer, so its allocations are reclaimed for free on
return - the standard scratch pattern:

```c
rc_view_myobj build_temp(rc_arena *a, rc_arena scratch) {
    rc_array_i32 tmp = rc_array_i32_make(256, scratch);  // local array backed by scratch arena
    rc_array_myobj result = {};                          // survives
      ...
    return result.view;                                  // view in permanent arena passed by caller
}
```

The struct is public: `rc_arena { char *base; uint32_t top, committed,
reserved; }`. Offsets are `uint32_t`, so one arena spans at most 4 GB. A zeroed
arena is the failure/empty state - check `base` after creation.

| API | Description |
|-----|-------------|
| `RC_MAX_ALIGN` | alignment of every allocation (alignment of `max_align_t`) |
| `RC_ARENA_DEFAULT_RESERVE` | default reservation size (256 MB) |
| `rc_arena_make(reserve_size) -> rc_arena` | reserve `reserve_size` bytes (page-rounded), commit the first page; a zeroed arena on failure - the one arena failure a caller checks |
| `rc_arena_make_default() -> rc_arena` | `rc_arena_make(RC_ARENA_DEFAULT_RESERVE)` |
| `rc_arena_deinit(a)` | release all virtual memory and zero the struct |
| `rc_arena_alloc(a, size) -> void *` | bump-allocate `size` bytes, uninitialised (freshly committed pages are OS-zeroed, but reclaimed space is not) |
| `rc_arena_alloc_zero(a, size) -> void *` | as `alloc`, but zeroed |
| `rc_arena_alloc_type(a, T, n)`<br>`rc_arena_alloc_zero_type(a, T, n)` | macros: allocate `n` elements of type `T`, cast to `T *` |
| `rc_arena_realloc(a, ptr, old_size, new_size) -> void *` | resize; the latest allocation grows in place (same address), older ones relocate by copy; NULL `ptr` acts like `alloc` |
| `rc_arena_realloc_zero(a, ptr, old_size, new_size) -> void *` | as `realloc`, zeroing the new bytes when growing |
| `rc_arena_free(a, ptr, size) -> bool` | reclaim `ptr` if it is the latest allocation (true); otherwise a no-op (false) |
| `rc_arena_free_to(a, offset)` | rewind `top` to an earlier mark, freeing everything after it in O(1) |
| `rc_arena_reset(a)` | rewind `top` to 0 and decommit every page but the first, returning physical memory to the OS |

The in-place growth of the latest allocation is what makes **one growable
container per arena** the optimal usage: when an array is its arena's sole
or final growable, growth never moves the buffer and raw pointers into it
survive. Although, as per the richc philosophy, we prefer indices to pointers.

---

## String view

`richc/str.h`. `rc_str` is a non-owning view over character data - a `const
char *` and a `uint32_t` length - passed and held by value, never allocating.
It is in one of three states: **invalid** `{NULL, 0}` (the "not found" /
"absent" sentinel), **empty** `{ptr, 0}`, or valid. Functions that operate on
the string assert their `rc_str` arguments are valid; the empty view is fully
supported. `rc_str_pair { rc_str first, second; }` is returned by the splits.

| API | Description |
|-----|-------------|
| `RC_STR(literal)` | compile-time view of a string literal (or `char[]`; not a `char *` - the length comes from `sizeof`) |
| `rc_str_make(data, len) -> rc_str` | view over an explicit pointer and length; need not be null-terminated |
| `rc_str_from_cstr(s) -> rc_str` | view over a null-terminated C string; invalid view when `s` is NULL |
| `rc_str_is_valid(s) -> bool` | `data` is non-NULL |
| `rc_str_is_empty(s) -> bool` | `len` is 0 (also true for the invalid view) |
| `rc_str_is_equal(a, b) -> bool` | same length and bytes; any two zero-length views are equal |
| `rc_str_is_equal_insensitive(a, b) -> bool` | as `is_equal`, folding ASCII case |
| `rc_str_compare(a, b) -> int` | byte-wise three-way compare; a prefix sorts first |
| `rc_str_compare_insensitive(a, b) -> int` | as `compare`, folding ASCII case |
| `rc_str_left(s, count)`<br>`rc_str_right(s, count) -> rc_str` | first / last `count` chars, clamped |
| `rc_str_substr(s, start, count) -> rc_str` | `count` chars from `start`, both clamped |
| `rc_str_skip(s, start) -> rc_str` | suffix from `start`, clamped |
| `rc_str_starts_with(s, prefix)`<br>`rc_str_ends_with(s, suffix) -> bool` | true for an empty prefix/suffix; false when longer than `s` |
| `rc_str_find_first(hay, needle)`<br>`rc_str_find_last(hay, needle) -> uint32_t` | match index or `RC_INDEX_NONE`; an empty needle is found at 0 / `hay.len` |
| `rc_str_contains(hay, needle) -> bool` | `find_first` succeeds |
| `rc_str_remove_prefix(s, prefix)`<br>`rc_str_remove_suffix(s, suffix) -> rc_str` | `s` with the affix removed when present, else unchanged |
| `rc_str_first_split(s, by)`<br>`rc_str_last_split(s, by) -> rc_str_pair` | split at the first/last delimiter, which neither half includes; delimiter absent -> `{s, invalid}` |
| `rc_str_as_cstr(s, buf, buf_size) -> const char *` | null-terminated C string: `s.data` directly when already followed by `'\0'` (no copy), else a truncated copy into `buf`; NULL when neither applies |

All slicing clamps out-of-range arguments rather than asserting.

---

## Mutable string

`richc/mstr.h`. `rc_mstr` is an arena-backed growable string, implemented like
`rc_array` but keeping a `'\0'` terminator at `data[len]`, so `rc_str_as_cstr`
on its view takes the no-copy fast path. Its `{data, len}` fields share layout
with `rc_str` through an anonymous union exposed as `s.view`, so a non-owning
view of the contents is always one field access away. `cap` is the real byte
capacity, holding up to `cap - 1` characters; growth is geometric (the larger
of `2*cap`, the request, or 8). `data == NULL` is the invalid zero-initialised
state; the mutators accept it and allocate on first use.

| API | Description |
|-----|-------------|
| `rc_mstr_make(capacity, arena) -> rc_mstr` | empty string in a `capacity`-byte buffer; 0 yields the invalid `{0}` (no allocation) |
| `rc_mstr_from_cstr(s, min_cap, arena)`<br>`rc_mstr_from_str(s, min_cap, arena) -> rc_mstr` | copy the source into a buffer of `max(len + 1, min_cap)` bytes; invalid input -> invalid result |
| `rc_mstr_is_valid(s) -> bool` | `data` is non-NULL |
| `rc_mstr_is_empty(s) -> bool` | `len` is 0 (also true when invalid) |
| `rc_mstr_reset(s)` | set `len` to 0, keeping the buffer |
| `rc_mstr_reserve(s, capacity, arena)` | ensure at least `capacity` bytes (exact); may move the buffer |
| `rc_mstr_append(s, str, arena)` | append an `rc_str`; empty is a no-op |
| `rc_mstr_append_char(s, c, arena)` | append one character |
| `rc_mstr_append_n(s, c, n, arena)` | append `n` copies of `c` (padding, rules); `n == 0` is a no-op |
| `rc_mstr_append_i32/i64/u32/u64(s, value, arena)` | append the integer in decimal (minus sign for negatives; no locale, no printf) |
| `rc_mstr_append_f32/f64(s, value, arena)` | append the float in `%g` form |
| `rc_mstr_append_hex8/16/32/64(s, value, arena)` | append as uppercase hex, zero-padded to the type's full width (2/4/8/16 digits); no prefix |
| `rc_mstr_replace(s, find, replacement, arena)` | replace every non-overlapping `find`, rewriting in place; empty `find` is a no-op |
| `rc_mstr_deinit(s, arena)` | free the backing (best-effort, see `rc_arena_free`) and zero to the invalid state; safe on an already invalid string |

---

## Container templates

`richc/template/`. Generic containers are preprocessor templates: define the
control macros, include the header, and it generates type-specific code, then
`#undef`s all its macros so it can be included again for another type. There is
no runtime polymorphism and no `void *` - the generated code is as specific and
optimisable as hand-written code.

```c
#define RC_ARRAY_TYPE int
#include "richc/template/array.h"
// now: rc_view_int, rc_span_int, rc_array_int and their operations
```

For the common element types the instantiation has already been done once in a
guarded header - see [Ready-made arrays](#ready-made-arrays) - so only
instantiate a template directly for your own types.

### richc/template/array.h - view, span, array

For one element type, generates a read-only view, a mutable span, and a
growable arena-backed array:

```c
rc_view_<s>   { const T *data; uint32_t num; }
rc_span_<s>   {       T *data; uint32_t num; }
rc_array_<s>  {       T *data; uint32_t num; uint32_t cap; }
```

The span and array embed an anonymous union, so narrowing conversions are
typesafe field accesses with no function call: `arr.view`, `arr.span`,
`span.view`. There is no allocating span/view constructor - allocation is the
array's job (resize an array and use the returned span, or `make_copy` a view).

| Control macro | Description |
|---------------|-------------|
| `RC_ARRAY_TYPE` | element type (required) |
| `RC_ARRAY_NAME` | name suffix `<s>` (optional; defaults to the type's own spelling, so give one for multi-token types) |
| `RC_ARRAY_DECLARE_ONLY` | emit only the typedefs (optional; for recursive element types that contain a view of themselves - declare, define the struct, then include again with `IMPL_ONLY`) |
| `RC_ARRAY_IMPL_ONLY` | emit only the functions, assuming a prior declare-only pass (optional) |

Shared macros, defined once and usable with any instantiation:

| API | Description |
|-----|-------------|
| `RC_AT(c, i)` | bounds-checked element access for any view/span/array; an lvalue; asserts `i < c.num` |
| `RC_VIEW(arr)`<br>`RC_SPAN(arr)` | brace initializers from a C array expression (declaration context only; `arr` must be a real array, not a pointer) |

Array operations (`rc_array_<s>_` prefix). Growing operations grow
geometrically - to the larger of double the capacity, the request, or 8 - so
they stay amortised O(1); `reserve` is exact. `arena` comes last and may be
NULL when no growth is needed.

| API | Description |
|-----|-------------|
| `_make(initial_capacity, arena) -> rc_array_<s>` | fresh array with exactly that capacity |
| `_make_copy(view, min_cap, arena) -> rc_array_<s>` | freshly allocated copy of a view |
| `_is_valid(a)`<br>`_is_empty(a) -> bool` | owns a buffer / `num == 0` |
| `_get(a, i) -> T`<br>`_set(a, i, v)`<br>`_at(a, i) -> T *` | element access; index asserted in range |
| `_reserve(a, capacity, arena)` | ensure exact capacity |
| `_resize(a, num, arena) -> rc_span_<s>` | set the element count; returns a span over the whole array |
| `_reset(a)` | `num = 0`, keep the buffer |
| `_deinit(a, arena)` | free the backing (best-effort) and zero the struct |
| `_push(a, v, arena) -> uint32_t` | append; returns the new element's index |
| `_push_n(a, n, arena)`<br>`_push_n_zero(a, n, arena) -> uint32_t` | append `n` uninitialised / zeroed elements; returns the first index |
| `_pop(a) -> T` | remove and return the last element; asserts non-empty |
| `_pop_n(a, n)` | drop the last `n` elements |
| `_append(a, view, arena) -> uint32_t` | append a whole view; returns the new element count |
| `_insert(a, i, v, arena)` | insert one element, shifting the tail |
| `_insert_n(a, i, n, arena)`<br>`_insert_n_zero(a, i, n, arena)` | insert `n` uninitialised / zeroed elements |
| `_remove(a, i)`<br>`_remove_n(a, i, n)` | remove, shifting the tail left |

Span operations (`rc_span_<s>_` prefix; the span is passed by value, writes go
through to the underlying memory):

| API | Description |
|-----|-------------|
| `_make(data, num) -> rc_span_<s>` | wrap a pointer and count; no allocation |
| `_is_valid(s)`<br>`_is_empty(s) -> bool` | `data != NULL` / `num == 0` |
| `_get_subspan(s, start, end)`<br>`_get_head(s, n)`<br>`_get_tail(s, n) -> rc_span_<s>` | sub-ranges, clamped |
| `_get(s, i) -> T`<br>`_set(s, i, v)`<br>`_at(s, i) -> T *` | element access |
| `_last_at(s) -> T *` | last element; asserts non-empty |
| `_reverse(s)` | reverse in place |
| `_rotate(s, k)` | left-rotate by `k` in place (Gries-Mills block swap: `num - gcd(num, k)` swaps, O(1) space); no-op when `k == 0` or `k >= num` |

View operations (`rc_view_<s>_` prefix): the same as span minus `set`,
`reverse`, and `rotate`; `_at` / `_last_at` return `const T *` and the slicing
helpers take and return views.

### richc/template/hash_trie.h - 16-way hash trie

An arena-backed map or set keyed by a 64-bit hash: each node has 16 children
selected by successive 4-bit groups of the hash. Nodes are stored 16 to a block
in an `rc_pool`, so a block emptied by `delete` is recycled, and one pool can
back many independent tries. A trie is a **value** - just a root block
reference - so `{0}` is a valid empty trie (no `make`), tries copy freely, and
the pool is passed to every operation rather than stored. Keys with identical
hashes chain correctly (access degrades to O(n) for n identical hashes).

| Control macro | Description |
|---------------|-------------|
| `RC_TRIE_KEY_TYPE` | key type (required) |
| `RC_TRIE_HASH(k)` | hash expression yielding a `uint64_t` (required) |
| `RC_TRIE_VALUE_TYPE` | define for a map; omit for a set (no `val` parameter, and `find` / `find_ptr` / `value_*` are not generated) |
| `RC_TRIE_EQUAL(a, b)` | key equality (optional; default `(a) == (b)`) |
| `RC_TRIE_NAME` | type name (optional; default `rc_trie_<KEY_TYPE>`, requiring a single-identifier key) |

Generated (`<t>` = the trie name): the pool type `<t>_pool` with
`<t>_pool_make(min_blocks, arena)`, `_pool_reserve`, `_pool_deinit` (from the
pool template; do not call the pool's low-level block ops on a pool that backs
tries), and:

| API | Description |
|-----|-------------|
| `<t> t = {0};` | construction: zero-initialise; the root block is allocated lazily by the first `add` |
| `<t>_contains(t, pool, key) -> bool` | membership (set and map) |
| `<t>_add(&t, pool, key[, val], arena) -> bool` | insert; true when the key was new (a map then replaces the value when false). The one op taking the trie by pointer |
| `<t>_delete(t, pool, key) -> bool` | remove; true when the key was present |
| `<t>_find(t, pool, key) -> uint32_t` | map only: node index or `RC_INDEX_NONE`; stable across pool growth, valid until the next add/delete |
| `<t>_find_ptr(t, pool, key) -> V *` | map only: value pointer or NULL; faster one-shot access, but subject to pool relocation |
| `<t>_value_get(pool, i) -> V`<br>`<t>_value_set(pool, i, v)`<br>`<t>_value_at(pool, i) -> V *` | map only: value access by node index |
| `<t>_key_get(pool, i) -> K`<br>`<t>_key_at(pool, i) -> const K *` | key read-back by node index (keys are immutable once placed) |

Read-only ops (`contains`, `find`, `value_get`, `key_get`, `key_at`) take a
`const` pool; only the mutators and the writable accessors take a mutable one.
Iteration is by the
[`hash_trie_foreach`](#richctemplatealgorithmhash_trie_foreachh---iterate-every-trie-entry)
template.

Several tries sharing one pool:

```c
#define RC_TRIE_KEY_TYPE   uint64_t
#define RC_TRIE_VALUE_TYPE uint32_t
#define RC_TRIE_HASH(k)    rc_hash_u64(k)
#define RC_TRIE_NAME       rc_trie_id
#include "richc/template/hash_trie.h"

rc_trie_id_pool pool = {0};     // one block store...
rc_trie_id materials = {0};     // ...backing any number of tries
rc_trie_id meshes    = {0};

rc_trie_id_add(&materials, &pool, material_id, mat_slot, &arena);
rc_trie_id_add(&meshes,    &pool, mesh_id,     mesh_slot, &arena);

uint32_t *slot = rc_trie_id_find_ptr(meshes, &pool, mesh_id);
```

Sharing the pool is what makes tries cheap in quantity. A trie is only a
4-byte root and an empty one touches no memory, so a map per material, per
entity, per whatever costs nothing until it holds something - where a
table-per-map design pays a header and an initial capacity for every map. All
the tries' nodes live in one backing array, so it takes one reserve and one
deinit for the lot, the pool can be its arena's sole growable (growing in
place, never moving), and lookups across many small maps stay inside one
contiguous, cache-friendly allocation instead of chasing per-map allocations.
And recycling crosses trie boundaries: blocks freed when one trie shrinks are
reused by whichever trie grows next, so total memory tracks the live node
count rather than the sum of per-map high-water marks.

### richc/template/pool.h - free-list object pool

An index-stable object pool backed by an `rc_array`: `alloc` hands out a stable
`uint32_t` index, and freed indices are recycled through an in-band free list
(each slot is a `union { uint32_t next_free_; T value; }`). References are
stored as `index + 1` with 0 meaning none, so a **zero-initialised pool is a
valid empty pool**. `free` always pushes the slot onto the free list, so
`alloc(); free(i);` restores the pool byte-for-byte; the backing holds its
high-water mark until `reset` / `deinit` reclaim it wholesale.

| Control macro | Description |
|---------------|-------------|
| `RC_POOL_TYPE` | element type (required) |
| `RC_POOL_NAME` | type name (optional; default `rc_pool_<TYPE>`) |

Generated (`<p>` = the pool name):

| API | Description |
|-----|-------------|
| `<p>_make(capacity, arena) -> <p>` | optional; only to pre-reserve (zero-init works) |
| `<p>_reserve(pool, min_capacity, arena)` | ensure backing capacity |
| `<p>_reset(pool)` | drop all elements, keep the backing |
| `<p>_deinit(pool, arena)` | free the backing and zero the struct |
| `<p>_alloc(pool, arena) -> uint32_t` | index of a zeroed slot; reuses a freed slot, else appends |
| `<p>_free(pool, index)` | recycle the slot (double-free is a caller error - use a genpool where that must trap) |
| `<p>_get(pool, i) -> T`<br>`<p>_set(pool, i, v)` | value access |
| `<p>_at(pool, i) -> T *`<br>`<p>_at_const(pool, i) -> const T *` | pointer access; survives in-place growth, invalidated by a relocating grow |
| `<p>_free_bitset(pool, arena) -> rc_bitset` | the liveness information: set bits are the dead (free-listed) slots |

The pool has no built-in iteration (a freed slot is byte-indistinguishable from
a live one) - use
[`pool_foreach`](#richctemplatealgorithmpool_foreachh---iterate-live-pool-entries).

### richc/template/genpool.h - generational object pool

Like `pool.h`, but each slot carries a generation counter outside the value
union, and `alloc` returns an
[`rc_genpool_handle`](#generational-pool-handle) instead of a bare index.
`free` bumps the slot's generation, invalidating every handle from the slot's
previous lifetime: a stale handle is *detected* (`is_valid` false, `at` NULL,
`get`/`set`/`free` assert) rather than silently aliasing the next occupant, and
double-free traps. Use a genpool where handles outlive their elements (resource
tables, scene objects); a plain pool where indices are managed strictly.
Zero-init is a valid empty pool.

| Control macro | Description |
|---------------|-------------|
| `RC_GENPOOL_TYPE` | element type (required) |
| `RC_GENPOOL_NAME` | type name (optional; default `rc_genpool_<TYPE>`) |

Generated (`<g>` = the genpool name):

| API | Description |
|-----|-------------|
| `<g>_make(capacity, arena) -> <g>` | optional; only to pre-reserve |
| `<g>_reserve(pool, min_capacity, arena)` | ensure backing capacity |
| `<g>_reset(pool)` | drop all elements, keep the backing (recreated slots restart at generation 0, so pre-reset handles can alias) |
| `<g>_deinit(pool, arena)` | free the backing and zero the struct |
| `<g>_alloc(pool, arena) -> rc_genpool_handle` | handle to a zeroed slot, carrying the slot's current generation |
| `<g>_free(pool, h)` | asserts `is_valid` (traps stale handles and double-free), bumps the generation, recycles the slot |
| `<g>_is_valid(pool, h) -> bool` | handle non-null, in range, and generation matches - the element is still live |
| `<g>_get(pool, h) -> T`<br>`<g>_set(pool, h, v)` | value access; assert `is_valid` |
| `<g>_at(pool, h) -> T *`<br>`<g>_at_const(pool, h) -> const T *` | non-trapping access; NULL for a null, out-of-range, or stale handle |
| `<g>_handle_at(pool, index) -> rc_genpool_handle` | the handle a slot currently validates against; call only for slots known live (on a free slot it forges the next-issue handle) |
| `<g>_free_bitset(pool, arena) -> rc_bitset` | set bits are the dead slots |

A slot's generation wraps after 2^32 frees, momentarily revalidating an ancient
handle - noted, not defended. Iteration is by
[`genpool_foreach`](#richctemplatealgorithmgenpool_foreachh---iterate-live-genpool-entries).

---

## Generational pool handle

`richc/genpool_handle.h`. The handle type handed out by the genpool template: a
slot index paired with the slot's generation at issue time. It lives in its own
small guarded header so a public header can declare handle-carrying types
without instantiating a pool. The fields are internal; `index_` stores the slot
index plus one with 0 meaning "no slot", so a **zero-initialised handle is the
null handle**. One handle type serves every genpool (not typesafe per pool);
where mixing pools must be a compile error, wrap the handle in a one-member
struct per pool (the app layer's gfx handles do this).

| API | Description |
|-----|-------------|
| `rc_genpool_handle` | `{ uint32_t index_; uint32_t gen_; }`; `{0}` is the null handle |
| `rc_genpool_handle_make(index, gen) -> rc_genpool_handle` | build a handle from a slot index and generation |
| `rc_genpool_handle_is_null(h) -> bool` | refers to no slot at all - NOT a liveness check (only the owning pool's `is_valid` is) |
| `rc_genpool_handle_index(h) -> uint32_t` | the slot index; asserts `h` is not null |
| `rc_genpool_handle_gen(h) -> uint32_t` | the generation at issue time |
| `rc_genpool_handle_equal(a, b) -> bool` | same slot and generation |

---

## Ready-made arrays

`richc/array/` and `richc/math/array/`. Rather than each translation unit
re-instantiating `template/array.h`, the view/span/array family for a common
element type is defined once in a guarded convenience header, so every includer
shares the same generated types. Include the header and use the family; the
API is exactly the [array template's](#richctemplatearrayh---view-span-array).

| Header | Types |
|--------|-------|
| `richc/array/u8.h` .. `u64.h`, `i8.h` .. `i64.h` | `rc_array_u8` .. `rc_array_u64`, `rc_array_i8` .. `rc_array_i64` (all four widths of each) |
| `richc/array/f32.h`, `richc/array/f64.h` | `rc_array_f32`, `rc_array_f64` |
| `richc/array/str.h`, `richc/array/mstr.h` | `rc_array_str`, `rc_array_mstr` |
| `richc/math/array/vec2i.h`, `vec3i.h`, `vec2f.h`, `vec3f.h`, `vec4f.h` | `rc_array_vec2i` etc. |
| `richc/math/array/box2i.h`, `box2f.h` | `rc_array_box2i`, `rc_array_box2f` |
| `richc/math/array/mat22f.h`, `mat23f.h`, `mat33f.h`, `mat34f.h`, `mat44f.h` | `rc_array_mat22f` etc. |
| `richc/math/array/quatf.h`, `rational.h` | `rc_array_quatf`, `rc_array_rational` |

(Each also provides the matching `rc_view_*` and `rc_span_*`.) The `uint8_t`
family is special-cased as [`bytes.h`](#byte-buffers), which adds the string
bridges.

---

## Byte buffers

`richc/bytes.h`. The array family instantiated for `uint8_t` under the name
`bytes` - `rc_view_bytes`, `rc_span_bytes`, `rc_array_bytes` with the full
[array template API](#richctemplatearrayh---view-span-array) - plus bridges to
the string types:

| API | Description |
|-----|-------------|
| `rc_view_bytes_as_str(bytes) -> rc_str` | reinterpret read-only bytes as an `rc_str`; no copy, no allocation, source stays usable |
| `rc_span_bytes_as_str(bytes) -> rc_str` | the same for a span |
| `rc_array_bytes_to_mstr(bytes, arena) -> rc_mstr` | **move** a byte buffer into an `rc_mstr`: appends a `'\0'` (growing only if exactly full) and resets `*bytes` to `{0}`, so only one mutable handle ever owns the buffer |

The `to_` reset is about ownership of *mutation*, not memory (the arena owns
that): two growable containers must never refer to one buffer, since either
could grow or rewrite it and corrupt the other. The read-only `as_` casts have
no such hazard.

---

## Bit array

`richc/bitset.h`. `rc_bitset { uint32_t *data; uint32_t num, cap; }` - a dense,
growable, arena-backed array of bits packed into `uint32_t` words (bit `i` is
word `i / 32`, position `i % 32`; `cap` is always a multiple of 32).
Zero-initialisation is a valid empty bitset. **Invariant:** every bit at a
position `>= num` is zero - every mutator preserves it, which lets the whole-word
operations below run with no per-bit masking. Growth is geometric, matching the
array policy; `arena` may be NULL whenever no growth is needed.

| API | Description |
|-----|-------------|
| `rc_bitset_reserve(bs, min_bits, arena)` | ensure capacity for `min_bits`, allocated exactly (word-rounded) |
| `rc_bitset_resize(bs, new_num, arena)` | set `num`; growing leaves new bits 0, shrinking zeroes the vacated bits |
| `rc_bitset_push(bs, val, arena) -> uint32_t` | append one bit; returns its index |
| `rc_bitset_push_n_zero(bs, n, arena) -> uint32_t` | append `n` zero bits; returns the first index |
| `rc_bitset_make_copy(src, arena) -> rc_bitset` | freshly allocated duplicate |
| `rc_bitset_set(bs, i)`<br>`rc_bitset_clear(bs, i)`<br>`rc_bitset_is_set(bs, i) -> bool` | single-bit access; assert `i < num` |
| `rc_bitset_reset(bs)` | clear all bits; `num`/`cap` unchanged |
| `rc_bitset_get_first_set(bs) -> uint32_t` | first set bit, or `RC_INDEX_NONE` |
| `rc_bitset_get_next_set(bs, pos) -> uint32_t` | first set bit at `>= pos`, or `RC_INDEX_NONE` |
| `rc_bitset_copy(dst, src)` | assign in place; the one op requiring equal widths (asserts `dst->num == src->num`) |
| `rc_bitset_union(dst, src)` | `dst \|= src`, capped to `dst->num` (src bits beyond it are dropped); not commutative |
| `rc_bitset_intersection(dst, src)` | `dst &= src`, capped to `dst->num` (dst bits beyond `src->num` clear); not commutative |
| `rc_bitset_intersects(a, b) -> bool` | share a set bit within `min(a->num, b->num)`; commutative |
| `rc_bitset_is_equal(a, b) -> bool` | identical; differing `num` -> false |
| `rc_bitset_num_set_bits(bs) -> uint32_t` | population count, computed on demand (not cached) |

Iterate set bits with the `get_first_set` / `get_next_set(i + 1)` idiom, or the
[`bitset_foreach`](#richctemplatealgorithmbitset_foreachh---iterate-set-bits)
template.

---

## Algorithm templates

`richc/template/algorithm/`. Each header is a preprocessor template generating
one function, and they all share one set of control-macro conventions - laid
out once here, so the per-file sections below only describe what each template
does. As with every template, all macros defined before inclusion are
undefined again by the header, so it can be included again for another
instantiation.

The comparator-based templates (sort, the binary searches, min/max element)
are parameterised on an element type:

| Control macro | Description |
|---------------|-------------|
| `RC_<X>_TYPE` | element type (required) |
| `RC_<X>_CMP(a, b)` | comparator expression, true iff `a < b` (optional; default `(a) < (b)`) |
| `RC_<X>_CTX` | context type (optional). Defining it adds a `CTX *` as the comparator's first argument - `RC_<X>_CMP(ctx, a, b)` - and as a function parameter |
| `RC_<X>_VIEW` / `RC_<X>_SPAN` | container type to operate on (optional; default `rc_view_<TYPE>` / `rc_span_<TYPE>`) |
| `RC_<X>_NAME` | generated function name (optional; default `rc_<x>_<TYPE>`) |

The `VIEW`/`SPAN` and `NAME` defaults paste `<TYPE>`, so a multi-token element
type needs explicit overrides.

```c
#define RC_SORT_TYPE int
#include "richc/template/algorithm/sort.h"
// void rc_sort_int(rc_span_int span);

typedef struct { int sign; } sign_ctx;
#define RC_SORT_TYPE          int
#define RC_SORT_CTX           sign_ctx
#define RC_SORT_CMP(c, a, b)  ((c)->sign * (a) < (c)->sign * (b))
#define RC_SORT_NAME          rc_sort_signed
#include "richc/template/algorithm/sort.h"
// void rc_sort_signed(rc_span_int span, sign_ctx *ctx);
```

The foreach templates (bitset, pool, genpool, hash trie) are parameterised on
a container type instead, and take a callback rather than a comparator:

| Control macro | Description |
|---------------|-------------|
| `RC_<X>_POOL` / `RC_<X>_TRIE` | container type name (required; drives the defaults). `bitset_foreach` has no container macro - there is only one `rc_bitset` type |
| `RC_<X>_FUNC(...)` | per-element callback macro (required) |
| `RC_<X>_CTX` | context type (optional). Defining it adds a `CTX *` as the callback's first argument and as a function parameter |
| `RC_<X>_NAME` | generated function name (optional; default `<container>_foreach`) |

### richc/template/algorithm/sort.h - introsort

In-place, not stable, over a mutable span: quicksort with a median-of-three
pivot, a heapsort fallback past depth `2*floor(log2(n))` (guaranteeing
`O(n log n)` worst case), and insertion sort for 16 elements or fewer - the
same strategy as libstdc++ and libc++.

| API | Description |
|-----|-------------|
| `rc_sort_<s>(span[, ctx])` | sort ascending under the comparator (pass a `>` comparator to sort descending) |

### richc/template/algorithm/lower_bound.h / upper_bound.h - binary search

Binary searches over a sorted `rc_view`. `lower_bound` returns the index of the
first element `>= value`; `upper_bound` the first strictly `> value` (reusing
the `<` comparator as `!(value < element)` - no second comparator needed); both
return `view.num` when no such element exists. With duplicates, `[lower,
upper)` is the equal range.

| API | Description |
|-----|-------------|
| `rc_lower_bound_<s>(view[, ctx], value) -> uint32_t` | first index whose element is `>= value` |
| `rc_upper_bound_<s>(view[, ctx], value) -> uint32_t` | first index whose element is `> value` |

### richc/template/algorithm/min_element.h / max_element.h - extremes

Scan an `rc_view` for the leftmost minimum or maximum under the comparison
(still a "less than" comparator; `max_element` applies it the other way round).

| API | Description |
|-----|-------------|
| `rc_min_element_<s>(view[, ctx]) -> uint32_t` | index of the first minimum, or `RC_INDEX_NONE` if empty |
| `rc_max_element_<s>(view[, ctx]) -> uint32_t` | index of the first maximum, or `RC_INDEX_NONE` if empty |

### richc/template/algorithm/bitset_foreach.h - iterate set bits

Visits the set bits of an [`rc_bitset`](#bit-array) in ascending order, calling
the callback macro on each index. Read-only; allocates nothing. There is no
`TYPE`, so the default name is fixed - give `RC_BITSET_FOREACH_NAME` to
generate more than one iterator in a translation unit.

| API | Description |
|-----|-------------|
| `NAME(bs[, ctx])` | call `RC_BITSET_FOREACH_FUNC([ctx,] index)` on each set bit; default name `rc_bitset_foreach` |

### richc/template/algorithm/pool_foreach.h - iterate live pool entries

Visits the *live* entries of an `rc_pool` (which cannot iterate itself): it
builds the dead-slot bitset via the pool's `free_bitset` in a by-value scratch
arena - discarded on return - then calls the callback with the pool and each
live slot's index. The callback reaches the object through the pool's
`get`/`set`/`at` and may mutate in place.

| API | Description |
|-----|-------------|
| `NAME(pool[, ctx], scratch)` | call `RC_POOL_FOREACH_FUNC([ctx,] pool, index)` on each live slot; `scratch` is an `rc_arena` by value |

### richc/template/algorithm/genpool_foreach.h - iterate live genpool entries

The genpool counterpart of `pool_foreach`: the same dead-slot-bitset walk, but
the callback receives each live element's `rc_genpool_handle` (reconstructed
via the pool's `handle_at`, so it satisfies `is_valid`) rather than a bare
index.

| API | Description |
|-----|-------------|
| `NAME(pool[, ctx], scratch)` | call `RC_GENPOOL_FOREACH_FUNC([ctx,] pool, handle)` on each live element; `scratch` as in `pool_foreach` |

### richc/template/algorithm/hash_trie_foreach.h - iterate every trie entry

Visits every entry of an `rc_trie` by walking from the root (one pool can back
many tries, so a flat pool scan could not tell them apart), calling the
callback with the pool and each entry's node index; reach the key and value
through the trie's `key_get` / `value_get`. Order is unspecified; iteration
allocates nothing (no scratch arena); do not add or delete keys during it. By
default the pool is mutable, so the callback may `value_set` in place; define
`RC_TRIE_FOREACH_CONST` for a read-only walk with a `const` pool.

| API | Description |
|-----|-------------|
| `NAME(t, pool[, ctx])` | call `RC_TRIE_FOREACH_FUNC([ctx,] pool, index)` on each entry; the trie goes by value (an empty trie visits nothing) |
| `RC_TRIE_FOREACH_CONST` | extra control macro, this template only: define for a read-only walk (const pool) |

---

## Hashing

`richc/hash.h`. `static inline` 32-bit hash functions for richc types, usable
directly as the hash expression for the hash-trie template. Float values `-0.0`
and `+0.0` are equal under `==`, so they are normalised to hash the same. The
vector hashes fold each component's scalar hash left-to-right with
`rc_hash_combine`, so component order matters.

| API | Description |
|-----|-------------|
| `rc_hash_u32(x)`<br>`rc_hash_i32(x) -> uint32_t` | Murmur3 32-bit finalizer |
| `rc_hash_u64(x)`<br>`rc_hash_i64(x) -> uint32_t` | splitmix64 finalizer, folded to 32 bits |
| `rc_hash_f32(x)`<br>`rc_hash_f64(x) -> uint32_t` | by bit pattern, zeros normalised |
| `rc_hash_ptr(p) -> uint32_t` | hashes the pointer value, not the pointee |
| `rc_hash_bytes(data, len) -> uint32_t` | FNV-1a over a byte range |
| `rc_hash_str(s) -> uint32_t` | hashes the string's bytes |
| `rc_hash_vec2i/vec3i/vec2f/vec3f/vec4f(v) -> uint32_t` | per-component fold |
| `rc_hash_combine(seed, hash) -> uint32_t` | mix one hash into a running seed (Boost formula), for hashing a struct field by field |

---

## Math

`richc/math/`. Vector, box, matrix, quaternion, and rational types plus
analytic root solvers. Everything is a small value type with `static inline`
operations (only `mat44f`'s determinant/inverse, `quatf`'s heavier functions,
`rational`, and `solve` have a `.c`); nothing allocates. Matrices are
**column-major** (`cx` is the first column; `m * v = cx*v.x + cy*v.y + ...`),
and `make_transpose` constructors take row vectors and transpose on store.

There is **one coordinate convention, never configurable** (the authoritative
statement is under the gfx section of [app.md](app.md)): 3D is right-handed
(+X right, +Y up, -Z forward), positive rotations follow the right-hand rule,
NDC is x right / y up in [-1, 1] with depth reversed into [0, 1] (near = 1),
and screen/image space is top-left origin, y down. No function takes a
handedness, depth-range, or winding parameter.

### richc/math/vec2i.h - 2D integer vector

`rc_vec2i { int32_t x, y; }`.

| API | Description |
|-----|-------------|
| `rc_vec2i_make(x, y)`, `_make_zero`, `_make_unitx`, `_make_unity` | constructors |
| `_from_i32s(p)`<br>`_as_i32s(a) -> const int32_t *` | from / as an `int32_t[2]` |
| `_add`, `_add3`, `_add4`, `_sub`, `_negate` | component sums and differences |
| `_scalar_mul(a, s)`, `_scalar_div(a, s)` | scalar scale (`div` asserts a non-zero divisor) |
| `_component_mul`, `_component_min`, `_component_max` | component-wise product / extremes |
| `_perp(a)` | the CCW perpendicular `(-y, x)` |
| `_dot`, `_wedge`, `_lengthsqr -> int64_t` | widened to `int64_t`, asserting no overflow; `wedge` is the 2D cross product. No `length` - the exact integer result is not representable |
| `_is_equal(a, b) -> bool` | exact |

### richc/math/vec3i.h - 3D integer vector

`rc_vec3i { int32_t x, y, z; }`.

| API | Description |
|-----|-------------|
| `rc_vec3i_make(x, y, z)`, `_make_zero`, `_make_unitx/y/z` | constructors |
| `_from_i32s(p)`<br>`_as_i32s(a)` | from / as an `int32_t[3]` |
| `_from_vec2i(v, z)` | extend a `rc_vec2i` |
| `_add`, `_add3`, `_add4`, `_sub`, `_negate`, `_scalar_mul`, `_scalar_div`, `_component_mul`, `_component_min`, `_component_max` | as `vec2i` |
| `_dot`, `_lengthsqr -> int64_t` | widened, overflow-asserted |
| `_cross(a, b) -> rc_vec3i` | each component computed in `int64_t` and asserted to fit `int32_t` |
| `_is_equal(a, b) -> bool` | exact |

### richc/math/vec2f.h - 2D float vector

`rc_vec2f { float x, y; }`.

| API | Description |
|-----|-------------|
| `rc_vec2f_make(x, y)`, `_make_zero`, `_make_unitx`, `_make_unity` | constructors |
| `_make_sincos(angle)`<br>`_make_cossin(angle)` | `(sin, cos)` / `(cos, sin)` of an angle in radians |
| `_from_floats(p)`<br>`_as_floats(a) -> const float *` | from / as a `float[2]` |
| `_from_vec2i(v)` | cast from `rc_vec2i` |
| `_add`, `_add3`, `_add4`, `_sub`, `_negate`, `_scalar_mul`, `_scalar_div`, `_component_mul`, `_component_min`, `_component_max` | component arithmetic |
| `_component_floor`, `_component_ceil`, `_component_abs` | per-component rounding / magnitude |
| `_lerp(a, b, t)` | `a + (b - a) * t` |
| `_perp(a)` | the CCW perpendicular `(-y, x)` |
| `_dot`, `_wedge`, `_lengthsqr`, `_length -> float` | scalar results; `wedge` is the 2D cross product |
| `_normalize(a)` | unit length; asserts non-zero |
| `_normalize_safe(a, tolerance)` | the zero vector when the length is below `tolerance`, instead of asserting |
| `_is_equal(a, b)`<br>`_is_nearly_equal(a, b, tolerance) -> bool` | exact / squared distance below `tolerance^2` |

### richc/math/vec3f.h - 3D float vector

`rc_vec3f { float x, y, z; }`. The same operation set as `vec2f` (minus
`_make_sincos`/`_make_cossin`/`_perp`/`_wedge`), plus:

| API | Description |
|-----|-------------|
| `rc_vec3f_make(x, y, z)`, `_make_zero`, `_make_unitx/y/z` | constructors |
| `_from_floats(p)`<br>`_as_floats(a)` | from / as a `float[3]` |
| `_from_vec2f(v, z)`<br>`_from_vec3i(v)` | extend a `rc_vec2f` / cast from `rc_vec3i` |
| `_cross(a, b) -> rc_vec3f` | the 3D cross product |
| `_add`, `_add3`, `_add4`, `_sub`, `_negate`, `_scalar_mul`, `_scalar_div`, `_component_mul/min/max/floor/ceil/abs`, `_lerp`, `_dot`, `_lengthsqr`, `_length`, `_normalize`, `_normalize_safe`, `_is_equal`, `_is_nearly_equal` | as `vec2f` |

### richc/math/vec4f.h - 4D float vector

`rc_vec4f { float x, y, z, w; }`. The same operation set as `vec3f` minus
`_cross`, plus:

| API | Description |
|-----|-------------|
| `rc_vec4f_make(x, y, z, w)`, `_make_zero`, `_make_unitx/y/z/w` | constructors |
| `_from_floats(p)`<br>`_as_floats(a)` | from / as a `float[4]` |
| `_from_vec2f(v, z, w)`<br>`_from_vec3f(v, w)` | extend a smaller vector |
| `_add`, `_add3`, `_add4`, `_sub`, `_negate`, `_scalar_mul`, `_scalar_div`, `_component_mul/min/max/floor/ceil/abs`, `_lerp`, `_dot`, `_lengthsqr`, `_length`, `_normalize`, `_normalize_safe`, `_is_equal`, `_is_nearly_equal` | as `vec3f` |

### richc/math/box2i.h - 2D integer box

`rc_box2i { rc_vec2i min_, max_; }` - an axis-aligned box over the half-open
region `[min, max)` (matching pixel/tile grids). `min <= max` component-wise is
an invariant the constructors establish and the queries assume; a
hand-initialised box must uphold it. The corners are internal members - read
them via the accessors.

| API | Description |
|-----|-------------|
| `rc_box2i_make(a, b)` | box from two corners, sorted |
| `_make_pos_size(pos, size)` | top-left plus extent |
| `_make_with_margin(a, b, margin)` | sorted corners expanded by `margin` per side; asserts no `int32_t` overflow |
| `_min(a)`<br>`_max(a) -> rc_vec2i` | the corners |
| `_size(a) -> rc_vec2i` | the extent `max - min` |
| `_is_empty(a) -> bool` | zero or negative extent on either axis |
| `_contains(a, b)`<br>`_contains_point(a, p)`<br>`_intersects(a, b) -> bool` | containment and overlap (touching edges do not intersect) |
| `_union(a, b)` | smallest box containing both |
| `_intersection(a, b)` | largest box contained in both; empty (`min == max`) when disjoint - test with `_is_empty` |
| `_expand(a, p)` | smallest box containing `a` and the point `p` |
| `_translate(a, delta)` | both corners shifted |
| `_is_equal(a, b) -> bool` | exact corner-wise equality |

### richc/math/box2f.h - 2D float box

`rc_box2f { rc_vec2f min_, max_; }` - the float counterpart of `rc_box2i`, with
the identical operation set over `rc_vec2f`, plus:

| API | Description |
|-----|-------------|
| `rc_box2f_is_nearly_equal(a, b, tolerance) -> bool` | both corners within `tolerance` |

### richc/math/mat22f.h - 2x2 float matrix

`rc_mat22f { rc_vec2f cx, cy; }`, column-major: `m * v = cx*v.x + cy*v.y`.

| API | Description |
|-----|-------------|
| `rc_mat22f_make(cx, cy)`, `_make_zero`, `_make_identity` | constructors |
| `_make_rotation(a)` | counter-clockwise rotation by `a` radians |
| `_from_floats(p)`<br>`_as_floats(m)` | from / as a column-major `float[4]` |
| `_add`, `_sub`, `_scalar_mul` | component arithmetic |
| `_vec2f_mul(m, v) -> rc_vec2f` | transform a vector (`m * v`) |
| `_mul(a, b)` | matrix product `a * b` |
| `_determinant(m) -> float`, `_transpose(m)` | |
| `_inverse(m)` | asserts determinant != 0 |

### richc/math/mat23f.h - 2D affine transform

`rc_mat23f { rc_mat22f rot; rc_vec2f trans; }` - a linear part and a
translation, applied as `rot * v + trans`.

| API | Description |
|-----|-------------|
| `rc_mat23f_make(rot, trans)`, `_make_identity`, `_make_translation(v)` | constructors |
| `_from_mat22f(m)` | embed a linear map with zero translation |
| `_from_floats(p)`<br>`_as_floats(m)` | from / as a `float[6]` (columns `rot.cx`, `rot.cy`, `trans`) |
| `_vec2f_mul(m, v) -> rc_vec2f` | `rot * v + trans` |
| `_mul(a, b)` | compose affine transforms |
| `rc_mat22f_mat23f_mul(L, b)`<br>`rc_mat23f_mat22f_mul(a, R)` | left / right multiply by a linear map (right leaves the translation unchanged) |
| `_inverse(m)` | delegates to `rc_mat22f_inverse`; asserts determinant != 0 |

### richc/math/mat33f.h - 3x3 float matrix

`rc_mat33f { rc_vec3f cx, cy, cz; }`, column-major.

| API | Description |
|-----|-------------|
| `rc_mat33f_make(cx, cy, cz)`, `_make_zero`, `_make_identity` | constructors |
| `_make_transpose(rx, ry, rz)` | from row vectors, transposing on store |
| `_make_rotation_x/y/z(a)` | right-handed rotation about each axis, radians |
| `_from_floats(p)`<br>`_as_floats(m)` | from / as a column-major `float[9]` |
| `_add`, `_sub`, `_scalar_mul`, `_vec3f_mul(m, v)`, `_mul(a, b)`, `_transpose` | as `mat22f` |
| `_determinant(m) -> float` | scalar triple product `cx . (cy x cz)` |
| `_inverse(m)` | adjugate/cofactor method; asserts determinant != 0 |

### richc/math/mat34f.h - 3D affine transform

`rc_mat34f { rc_mat33f rot; rc_vec3f trans; }`, applied as `rot * v + trans`.

| API | Description |
|-----|-------------|
| `rc_mat34f_make(rot, trans)`, `_make_identity`, `_make_translation(v)` | constructors |
| `_make_lookat(eye, focus, up)` | right-handed view matrix mapping the eye to the origin with -Z toward `focus`; `up` is orthogonalised |
| `_from_mat33f(m)` | embed a linear map with zero translation |
| `_from_floats(p)`<br>`_as_floats(m)` | from / as a `float[12]` |
| `_vec3f_mul(m, v)`, `_mul(a, b)` | transform / compose |
| `rc_mat33f_mat34f_mul(L, b)`<br>`rc_mat34f_mat33f_mul(a, R)` | left / right multiply by a linear map |
| `_inverse(m)` | delegates to `rc_mat33f_inverse`; asserts determinant != 0 |

### richc/math/mat44f.h - 4x4 float matrix

`rc_mat44f { rc_vec4f cx, cy, cz, cw; }`, column-major. Inline except
`determinant` and `inverse` (in `src/math/mat44f.c`). The projections all
target the library's one canonical clip space - NDC x right and y up in
[-1, 1], depth in [0, 1] with reverse-Z (near -> 1, far -> 0), view space
right-handed with -Z forward - and there are no handedness or depth-range
variants.

| API | Description |
|-----|-------------|
| `rc_mat44f_make(cx, cy, cz, cw)`, `_make_zero`, `_make_identity` | constructors |
| `_make_transpose(rx, ry, rz, rw)` | from row vectors, transposing on store |
| `_make_translation(v)` | 3D translation |
| `_make_perspective(y_fov, aspect, n, f)` | finite far plane; view z = -n gives depth exactly 1, z = -f exactly 0 |
| `_make_perspective_inf(y_fov, aspect, n)` | the f -> infinity limit (`depth = n / -z`); the default to reach for in 3D |
| `_make_ortho(left, right, top, bottom, n, f)` | maps the box to NDC, depth sense reversed |
| `_make_ortho_2d(w, h)` | 2D convenience: a pixel rect, top-left origin, y down; z = 0 lands on depth 1. Equals `_make_ortho(0, w, 0, h, 0, 1)` |
| `_from_mat22f/33f/34f(m)` | embed a smaller matrix |
| `_from_floats(p)`<br>`_as_floats(m)` | from / as a column-major `float[16]` |
| `_add`, `_sub`, `_scalar_mul`, `_vec4f_mul(m, v)`, `_mul(a, b)`, `_transpose` | as the smaller matrices |
| `_determinant(m) -> float`, `_inverse(m)` | in the `.c`; `inverse` asserts determinant != 0 |

### richc/math/quatf.h - quaternion

`rc_quatf { rc_vec3f xyz; float w; }` - a rotation as `q = w + x*i + y*j + z*k`
(Hamilton convention), identity `(0,0,0,1)`. The rotation-building constructors
return unit quaternions; the component arithmetic does not preserve unit
length, so normalise when a unit result is needed. Cheap value ops are inline;
`make_angle_axis`, the matrix conversions, `slerp`, `exp`, `log`, and `pow`
live in `src/math/quatf.c`.

| API | Description |
|-----|-------------|
| `rc_quatf_make(x, y, z, w)`, `_make_identity` | constructors |
| `_make_angle_axis(angle, axis)` | rotation about an axis (normalised internally) |
| `_from_floats(p)`<br>`_as_floats(q)` | from / as a `float[4]` (`x,y,z,w`) |
| `_from_vec3f(xyz, w)` | from vector and scalar parts |
| `_from_mat33f(m)`<br>`rc_mat33f_from_quatf(q)` | rotation matrix conversion both ways (`from_mat33f` uses Mike Day's branch method - one sqrt, stable through 180-degree turns) |
| `_add`, `_sub`, `_scalar_mul`, `_negate`, `_dot`, `_lengthsqr`, `_length`, `_normalize` | 4-vector component arithmetic (not unit-preserving) |
| `_conjugate(q)` | = inverse for unit `q` |
| `_inverse(q)` | conjugate / `\|q\|^2` |
| `_mul(a, b)` | compose rotations; `b` applied first |
| `_vec3f_transform(q, v)` | rotate `v` (Rodrigues formula, no matrix) |
| `_angle(q) -> float`<br>`_axis(q) -> rc_vec3f` | angle-axis read-back (unit X when there is no rotation) |
| `_lerp`, `_nlerp` | linear / normalised-linear interpolation |
| `_slerp(a, b, t)` | shorter-arc spherical interpolation; falls back to nlerp when nearly parallel |
| `_exp`, `_log`, `_pow(q, t)` | exponential maps (`log` asserts `\|q\| != 0`; `pow` is `exp(t * log(q))`) |
| `_is_equal`, `_is_nearly_equal(a, b, tolerance) -> bool` | comparison |

### richc/math/rational.h - rational arithmetic

`rc_rational { int64_t num_, denom_; }` - an exact rational, always held in
canonical form (`denom > 0`, `gcd(|num|, denom) == 1`); the members are
internal, so read them through the accessors and build values through the
constructors. Division by zero produces the invalid state `0/0`. Operations
assert their inputs are valid and their results fit `int64_t`; GCD
pre-reduction keeps intermediates small, but genuine overflow asserts rather
than wrapping.

| API | Description |
|-----|-------------|
| `rc_rational_make(num, denom)` | canonicalises; `denom == 0` -> invalid |
| `_from_i64(n)` | the integer `n/1` |
| `_from_double(val, threshold)` | simplest rational within `threshold` of `val` (continued fractions) |
| `_num(a)`<br>`_denom(a) -> int64_t` | the canonical numerator / denominator |
| `_is_valid`, `_is_zero`, `_is_integer`, `_is_positive`, `_is_negative -> bool` | predicates |
| `_negate`, `_abs`, `_reciprocal` | unary |
| `_add`, `_sub`, `_mul`, `_div` | rational-rational arithmetic |
| `_int_add`, `_int_sub`, `_int_mul`, `_int_div` | variants taking an `int64_t` second operand |
| `_compare(a, b) -> int32_t` | -1/0/+1; overflow-safe (a continued-fraction descent that never forms the cross product) |
| `_is_equal`, `_is_less_than`, `_is_greater_than -> bool`, `_min`, `_max` | comparisons built on it |
| `_to_double(a) -> double` | approximate conversion |

### richc/math/solve.h - polynomial root solvers

Analytic real-root solvers (in `src/math/solve.c`), each returning a small
by-value struct with the count and the roots, in no particular order.
Degenerate leading coefficients fall back to the lower-degree solver rather
than asserting.

| API | Description |
|-----|-------------|
| `rc_solve_quadratic(a, b, c) -> rc_quadratic_roots` | `{ int num_roots; float root[2]; }` - 0-2 real roots of `a*t^2 + b*t + c`; sign-selection plus Vieta avoids catastrophic cancellation |
| `rc_solve_cubic(a, b, c, d) -> rc_cubic_roots` | `{ int num_roots; float root[3]; }` - 1 or 3 real roots (Cardano / trigonometric method) |

---

## Rectangle packing

`richc/rect_pack.h`. Packs axis-aligned rectangles into a container with a
spacing gap, using Maximal Rectangles with Best Short Side Fit. It works purely
on sizes and positions - no image dependency - so it underpins the app layer's
image atlas packers but is reusable for any 2D packing. Results are returned by
value or as a span over the output arena; the packer captures no arena.
`spacing` inflates each placed rectangle before carving the free list; no
border gap is enforced at the container edges.

| API | Description |
|-----|-------------|
| `rc_rect_pack_all(container, spacing, sizes, arena, scratch) -> rc_span_vec2i` | from-scratch batch path: sorts the `rc_view_vec2i` sizes by decreasing longer side for density, places them all, and returns positions indexed by the input (allocated from `arena`) - or the invalid `{0}` span if the whole set does not fit, leaving `arena` untouched. `scratch` is a by-value arena (necessarily a *different* arena) for the free list and sort permutation |
| `rc_rect_pack_make(container, spacing, arena) -> rc_rect_pack` | incremental path: a packer that retains its free-rect list as state |
| `rc_rect_pack_add(packer, size, arena) -> rc_rect_pack_result` | place one rectangle without disturbing earlier placements; returns `{ rc_vec2i pos; bool placed; }` (`placed` false when it no longer fits). Pass the same arena as `make`, and let the free list be that arena's only growable |

---

## Random numbers

`richc/random.h`. A tiny, fast pseudo-random generator (splitmix32): one word
of state, an add and two multiply-mixes per draw. Good statistical quality and
no bad seeds (any seed, including 0, is fine); deterministic, so a seed always
replays the same stream. Not for cryptography.

| API | Description |
|-----|-------------|
| `rc_random_make(seed) -> rc_random` | a generator (`{ uint32_t state; }`) seeded to `seed`; reseed by assigning a fresh one |
| `rc_random_next(p) -> uint32_t` | the next 32-bit draw, advancing the state. Bound with `% n` (bias negligible for small `n`) |

---

## Scalar operations

`richc/ops.h`. Small `static inline` scalar helpers. Functions carry a scalar
type suffix (`i32`/`i64`/`u32`/`u64`/`f32`/`f64`), which also dodges the
Windows `min`/`max` macros.

| API | Description |
|-----|-------------|
| `rc_bitcast_f32(x) -> uint32_t`<br>`rc_bitcast_f64(x) -> uint64_t` | the float's bit pattern (via a union); used by the float hashes |
| `rc_min_i32/i64/f32/f64(a, b)`<br>`rc_max_i32/i64/f32/f64(a, b)` | minimum / maximum |
| `rc_sgn_i32/i64(a)` | -1, 0, or +1 |
| `rc_gcd_i32/i64(a, b)` | Euclidean GCD, always non-negative |
| `rc_clz_u32/u64(a)`<br>`rc_ctz_u32/u64(a) -> uint32_t` | count leading / trailing zeros (the type's width for 0) |
| `rc_popcount_u32(a) -> uint32_t` | population count |
| `rc_mul_overflows_u64`, `rc_add_overflows_u64`, `rc_add_overflows_i64`, `rc_sub_overflows_i64`, `rc_mul_overflows_i64 -> bool` | true when the operation would overflow the result type |
| `rc_deg_to_rad(degrees) -> float` | degrees to radians |

---

## File I/O

`richc/file.h`. Whole-file load and save. Filenames are `rc_str`; all I/O is
binary mode (no line-ending translation), and loaded data is allocated from the
supplied arena. Every function reports an `rc_file_error`: `RC_FILE_OK` (0),
`RC_FILE_ERROR_NOT_FOUND`, `RC_FILE_ERROR_ACCESS_DENIED`,
`RC_FILE_ERROR_TOO_LARGE`, or `RC_FILE_ERROR_IO`. Loads return a mutable,
growable result - an `rc_mstr` for text, an `rc_array_bytes` for binary -
because loaded data is commonly modified after reading; a read-only caller just
takes `.view`. `minimum_capacity` is a byte-capacity floor on the result
buffer, which is `max(size + 1, minimum_capacity)` bytes - the spare byte
becomes the text load's `'\0'` terminator (so `rc_str_as_cstr` takes its
no-copy fast path).

| API | Description |
|-----|-------------|
| `rc_file_size(filename) -> rc_file_size_result` | `{ uint32_t size; rc_file_error error; }` - byte size without reading; `TOO_LARGE` if it does not fit a `uint32_t`. No arena |
| `rc_file_exists(filename) -> bool` | presence only; no arena, no read access needed |
| `rc_file_load_text(filename, min_cap, arena) -> rc_file_load_text_result` | `{ rc_mstr text; rc_file_error error; }`; on failure `text` is the invalid state |
| `rc_file_load_binary(filename, min_cap, arena) -> rc_file_load_binary_result` | `{ rc_array_bytes contents; rc_file_error error; }` |
| `rc_file_save_text(filename, text) -> rc_file_error` | create or truncate, write an `rc_str` |
| `rc_file_save_binary(filename, data) -> rc_file_error` | create or truncate, write an `rc_view_bytes` |
| `rc_file_delete(filename) -> rc_file_error` | remove; `NOT_FOUND` when it does not exist |

---

## Decompression

`richc/zip/inflate.h`. DEFLATE decompression into a growable byte array
(decompression only; there is no compressor). Compressed input is untrusted:
malformed data returns an error, never traps. Both entry points accept stored,
fixed, and dynamic Huffman blocks and share the result type
`rc_zip_inflate_result { rc_array_bytes data; rc_zip_error error; }`, where
`rc_zip_error` is `RC_ZIP_OK`, `RC_ZIP_ERROR_BAD_DATA`,
`RC_ZIP_ERROR_TRUNCATED`, `RC_ZIP_ERROR_BAD_HEADER`, or
`RC_ZIP_ERROR_CHECKSUM`. The output is owned by the supplied arena; on error
`data` is the empty state. `minimum_capacity` pre-sizes the output for a caller
that knows the decompressed size (DEFLATE does not record it; pass 0 when
unknown).

| API | Description |
|-----|-------------|
| `rc_zip_inflate(compressed, min_cap, arena) -> rc_zip_inflate_result` | decode a raw DEFLATE stream (RFC 1951) |
| `rc_zip_inflate_zlib(compressed, min_cap, arena) -> rc_zip_inflate_result` | decode a zlib-wrapped stream (RFC 1950): validates the 2-byte header, rejects a preset dictionary, verifies the trailing Adler-32 |

---

## Time

`richc/time.h`. A single steady, monotonic nanosecond clock
(QueryPerformanceCounter / `clock_gettime(CLOCK_MONOTONIC)`). It never goes
backwards and is unaffected by wall-clock changes, so only differences between
readings are meaningful - use it for elapsed-time measurement and timeout
deadlines (it pairs with the `wait_for` timeouts in `thread/`).

| API | Description |
|-----|-------------|
| `rc_time_now_ns() -> uint64_t` | current value of the monotonic clock, in nanoseconds; the origin is arbitrary and platform-defined |

---

## Concurrency

`richc/thread/`. Hand-rolled cross-platform concurrency primitives - a
replacement for C11 threads/atomics, which are not reliably available under
MSVC - and a task-graph thread pool built on top of them. Atomics split by
*compiler* (clang/gcc builtins vs MSVC intrinsics); the sync and thread objects
split by *OS* (Win32 vs POSIX, with Apple's `dispatch_semaphore` for the
semaphore). Unusually for the library, these public headers **include the OS
header** to embed the real platform type as a private `handle_` member - a
deliberate, scoped exception to the "no OS headers in the public API" rule,
because the objects must be constructed in place in their final storage and are
not copyable. They are initialised with `_init` / cleaned up with `_deinit`
rather than returned by value, for the same reason. OS calls that can only fail
on programmer error `RC_PANIC`; only `rc_thread_create` returns a failure.

### richc/thread/atomic.h - atomics

Lock-free atomic scalars with explicit memory ordering. Each atomic type wraps
one value in a struct with a private member; all access goes through the typed
operations, every one of which takes an explicit `rc_memory_order` -
`RC_MEMORY_ORDER_RELAXED`, `_ACQUIRE`, `_RELEASE`, `_ACQ_REL`, or `_SEQ_CST`
(consume is deliberately omitted). Backends: clang/gcc use the `__atomic_*`
builtins with full order support; MSVC uses `_Interlocked*` intrinsics, which
are full-barrier, so every order maps to sequential consistency - always
correct, if stronger than asked for.

Types: `rc_atomic_u8/i8/u16/i16/u32/i32/u64/i64` (integers), `rc_atomic_ptr`
(`void *`), `rc_atomic_bool`, and `rc_atomic_flag` (test-and-set flag). In the
table `<T>` is the integer type suffix and `T` its value type.

| API | Description |
|-----|-------------|
| `rc_atomic_<T>_load(a, order) -> T`<br>`rc_atomic_<T>_store(a, v, order)` | atomic read / write |
| `rc_atomic_<T>_exchange(a, v, order) -> T` | swap; returns the previous value |
| `rc_atomic_<T>_compare_exchange_strong/weak(a, &expected, desired, order) -> bool` | CAS; on failure writes the observed value back to `expected` (`weak` may fail spuriously - use in a loop) |
| `rc_atomic_<T>_fetch_add/sub/and/or/xor(a, v, order) -> T` | read-modify-write; return the previous value |
| `rc_atomic_ptr_*`<br>`rc_atomic_bool_*` | load / store / exchange / compare_exchange only (no arithmetic; `bool` has `compare_exchange_strong`) |
| `rc_atomic_flag_test_and_set(f, order) -> bool`<br>`rc_atomic_flag_clear(f, order)` | set and return the previous state / clear |
| `rc_atomic_thread_fence(order)` | full inter-thread fence |
| `rc_atomic_signal_fence(order)` | compiler-only fence (same thread / signal handler) |

### richc/thread/spinlock.h - spinlock

A minimal test-and-set lock over `rc_atomic_flag`: header-only, no OS object.
For very short critical sections where a mutex's syscall on contention would
dominate; a contended waiter burns CPU, so never hold it across blocking or
lengthy work. A zero-initialised `rc_spinlock` is a valid unlocked lock - no
init call.

| API | Description |
|-----|-------------|
| `rc_spinlock_lock(s)` | acquire, spinning until free |
| `rc_spinlock_trylock(s) -> bool` | acquire without spinning; true if taken |
| `rc_spinlock_unlock(s)` | release |

### richc/thread/mutex.h - mutexes

`rc_mutex` is a non-recursive lock (relocking by the owner is undefined) - the
lock that pairs with `rc_cond`. `rc_mutex_recursive` may be relocked by its
owning thread, each lock matched by an unlock; it does not pair with `rc_cond`.
Backing: SRWLOCK / CRITICAL_SECTION on Windows, pthread mutexes elsewhere.
Using a mutex after deinit, or destroying a locked one, is undefined.

| API | Description |
|-----|-------------|
| `rc_mutex_init(m)`<br>`rc_mutex_deinit(m)` | construct / destroy in place |
| `rc_mutex_lock(m)`<br>`rc_mutex_unlock(m)` | acquire / release |
| `rc_mutex_trylock(m) -> bool` | true if the lock was acquired |
| `rc_mutex_recursive_init/deinit/lock/trylock/unlock` | the recursive counterpart |

### richc/thread/rwlock.h - reader/writer lock

`rc_rwlock` allows any number of concurrent readers or a single exclusive
writer (SRWLOCK / pthread_rwlock). Not recursive; a read lock cannot be
upgraded (release, then re-acquire). Read and write locks are released by their
own calls, matching the SRWLOCK model.

| API | Description |
|-----|-------------|
| `rc_rwlock_init(rw)`<br>`rc_rwlock_deinit(rw)` | construct / destroy in place |
| `rc_rwlock_read_lock/read_trylock/read_unlock(rw)` | shared acquisition (`trylock -> bool`) |
| `rc_rwlock_write_lock/write_trylock/write_unlock(rw)` | exclusive acquisition (`trylock -> bool`) |

### richc/thread/cond.h - condition variable

`rc_cond` lets a thread wait, with its `rc_mutex` released, until another
thread signals a change (CONDITION_VARIABLE / pthread_cond; on Linux built on
CLOCK_MONOTONIC, so timeouts ignore wall-clock changes). Waits may wake
spuriously, so always re-check the predicate in a loop while holding the mutex.
Pairs only with the non-recursive `rc_mutex`. The classic pattern:

```c
rc_mutex_lock(&m);
while (!predicate)
    rc_cond_wait(&c, &m);   // atomically unlocks m, sleeps, re-locks m
// predicate now holds, m held
rc_mutex_unlock(&m);
```

| API | Description |
|-----|-------------|
| `rc_cond_init(c)`<br>`rc_cond_deinit(c)` | construct / destroy in place |
| `rc_cond_wait(c, m)` | atomically unlock `m`, sleep, re-lock `m`; `m` must be held on entry |
| `rc_cond_wait_for(c, m, timeout_ns) -> bool` | as `wait`, giving up after `timeout_ns`; false if the timeout elapsed. `m` is held on return either way |
| `rc_cond_signal(c)` | wake at least one waiter |
| `rc_cond_broadcast(c)` | wake all waiters |

### richc/thread/semaphore.h - counting semaphore

`rc_semaphore` holds a non-negative count: `wait` decrements it, blocking while
it is zero; `post` increments by a given amount, releasing that many waiters.
Useful for bounding a resource pool or waking a worker queue. Backing: Win32
semaphore, POSIX `sem_t` on Linux, `dispatch_semaphore_t` on Apple. (The Linux
`sem_timedwait` deadline is CLOCK_REALTIME - the one timeout here that is not
monotonic.)

| API | Description |
|-----|-------------|
| `rc_semaphore_init(s, initial_count)`<br>`rc_semaphore_deinit(s)` | construct / destroy in place |
| `rc_semaphore_wait(s)` | block until the count is positive, then decrement |
| `rc_semaphore_try_wait(s) -> bool` | decrement if positive; true if it did |
| `rc_semaphore_wait_for(s, timeout_ns) -> bool` | false if the timeout elapsed first |
| `rc_semaphore_post(s, count)` | add `count` to the semaphore |

### richc/thread/thread.h - threads and utilities

`rc_thread` is caller-owned and initialised in place: it stores the function
and argument and is itself handed to the OS thread, so launching one allocates
nothing. Because the running thread reads through the object, it **must stay
live at a stable address until joined** (or, if detached, for the thread's
lifetime) - store it somewhere durable, not a temporary.

| API | Description |
|-----|-------------|
| `rc_thread_create(t, fn, arg) -> bool` | start a thread running `fn(arg)` (`rc_thread_func` is `void (*)(void *)`); false if the OS refused - the one legitimate creation failure |
| `rc_thread_join(t)` | wait for the thread and release its resources |
| `rc_thread_detach(t)` | release the thread to run independently; it can no longer be joined |
| `rc_thread_current_id() -> uint64_t` | opaque id, unique among live threads |
| `rc_thread_yield()` | hint the OS scheduler to run another thread |
| `rc_thread_sleep_ns(ns)` | sleep for at least `ns` nanoseconds |
| `rc_thread_hardware_concurrency() -> uint32_t` | logical core count (>= 1); for sizing a pool |
| `rc_once`<br>`rc_once_run(once, fn)` | run `fn()` exactly once across all threads; a zero-initialised `rc_once` is ready to use |
| `RC_THREAD_LOCAL` | keyword macro declaring a variable with per-thread storage |

One worker per core over a shared atomic - the `rc_thread` objects live in an
array that outlives the threads (the stable-storage rule), and each is joined
before the array goes away:

```c
typedef struct worker_ctx {
    rc_atomic_i64 *counter;
    int            iters;
} worker_ctx;

static void worker(void *p)
{
    worker_ctx *c = p;
    for (int i = 0; i < c->iters; ++i)
        rc_atomic_i64_fetch_add(c->counter, 1, RC_MEMORY_ORDER_RELAXED);
}

rc_atomic_i64 counter = {0};
worker_ctx ctx = {.counter = &counter, .iters = 100000};

uint32_t num = rc_thread_hardware_concurrency();
rc_thread *threads = rc_arena_alloc_type(&arena, rc_thread, num);
for (uint32_t i = 0; i < num; ++i)
    RC_PANIC(rc_thread_create(&threads[i], worker, &ctx));
for (uint32_t i = 0; i < num; ++i)
    rc_thread_join(&threads[i]);
// counter == num * ctx.iters
```

### richc/thread/tls.h - thread-local keys

`rc_tls` is a dynamically created key naming one `void *` slot with an
independent value per thread (TlsAlloc / pthread_key) - for when the set of
thread-local variables is not known at compile time (`RC_THREAD_LOCAL` is
simpler when it is). Slots start NULL in every thread; keys carry no
destructor. The key is a small copyable handle, so it is returned and passed by
value.

| API | Description |
|-----|-------------|
| `rc_tls_create() -> rc_tls` | allocate a key |
| `rc_tls_destroy(key)` | release the key; using it afterwards is undefined |
| `rc_tls_get(key) -> void *`<br>`rc_tls_set(key, value)` | this thread's value for the key |

### richc/thread/scheduler.h - task scheduler

`rc_scheduler` (opaque) is a task-graph thread pool: worker threads plus a
bounded pool of task slots. A task is a function and a `void *` context; wire
tasks into a dependency graph, submit them, and wait. Like everything in richc
the scheduler never owns its allocation: `create` carves the scheduler and all
its buffers from an arena in one shot (the block never grows, so it may share
an arena), and the returned pointer is stable for the arena's lifetime - which,
note, makes this the library's one `_create` (it returns a pointer, so it is
not a by-value `_make`). `RC_PANIC` on task-pool exhaustion - size `max_tasks`
generously.

Types:

| Type | Description |
|------|-------------|
| `rc_task` | a task handle: `{ uint32_t slot, generation; }`; the generation traps stale handles. `RC_TASK_NONE` is the null handle |
| `rc_task_func` | `void (*)(rc_task_context *tc, void *ctx)` |
| `rc_task_context` | passed to the running task: the `scheduler` (to spawn children), a per-worker `scratch` arena reset after the task returns, the task's own handle `self`, and `worker_index` |
| `rc_scheduler_config` | `num_threads` (0 -> hardware concurrency - 1, at least 1), `max_tasks` (the bound on outstanding tasks), `scratch_reserve` (per-worker scratch VA reserve; 0 -> a default) |
| `RC_TASK_RESULT_SIZE` | size (64) of each slot's inline result buffer; fixed ABI shared with the future template |

Functions:

| API | Description |
|-----|-------------|
| `rc_scheduler_create(config, arena) -> rc_scheduler *` | carve the scheduler from `arena` and start the workers |
| `rc_scheduler_deinit(s)` | stop and join the workers and destroy the scratch arenas; never frees the block (the arena owner reclaims it) |
| `rc_scheduler_task_make(s, fn, ctx) -> rc_task` | create a task (not yet runnable) holding a single self-hold |
| `rc_task_after(s, before, run_after)` | make `run_after` wait for `before`; wire before submitting |
| `rc_scheduler_submit(s, task)` | release the self-hold; the task runs once all predecessors have completed (wiring order vs submit order does not matter) |
| `rc_scheduler_run(s, fn, ctx) -> rc_task` | make + submit with no dependencies (fire-and-forget) |
| `rc_scheduler_wait(s, task)` | wait for a task. On a worker it *participates* - running other ready tasks while it waits - so recursive fork/join keeps the cores busy and cannot deadlock the pool; on an outside thread it blocks |
| `rc_scheduler_wait_all(s)` | wait (from an outside thread) until all submitted work has drained |
| `rc_task_context_result(tc) -> void *` | where the running task's result goes: the caller's `out` storage when a future supplied one, else the inline slot buffer. The typed future accessors build on this |
| `rc_scheduler_run_future_`, `rc_scheduler_get_result_` | internal (trailing `_`): used by the future template; not part of the API |

A task can equally skip results and futures entirely and just write through a
caller struct reached via its `ctx`.

Wiring a dependency graph - `b` and `c` run in parallel after `a`, `d` after
both (wiring order versus submit order does not matter, so submit them all and
wait on the last):

```c
rc_arena arena = rc_arena_make_default();
rc_scheduler *s = rc_scheduler_create(
    (rc_scheduler_config) {.max_tasks = 4096}, &arena);

rc_task a = rc_scheduler_task_make(s, load,  &job);
rc_task b = rc_scheduler_task_make(s, parse, &job);
rc_task c = rc_scheduler_task_make(s, hash,  &job);
rc_task d = rc_scheduler_task_make(s, store, &job);
rc_task_after(s, a, b);
rc_task_after(s, a, c);
rc_task_after(s, b, d);
rc_task_after(s, c, d);
rc_scheduler_submit(s, a);
rc_scheduler_submit(s, b);
rc_scheduler_submit(s, c);
rc_scheduler_submit(s, d);
rc_scheduler_wait(s, d);
```

Recursive fork/join - a task spawns children through `tc->scheduler` and waits
on them; because `wait` on a worker participates (running other ready tasks),
the recursion keeps every core busy and cannot deadlock the pool. The child
jobs live in `tc->scratch`, the per-worker scratch arena: it is reset only
after the task returns, and the parent waits for both children before
returning, so no cleanup is needed:

```c
typedef struct sum_job {
    const int *v;
    uint32_t   lo;
    uint32_t   hi;
    int64_t    out;
} sum_job;

static void parallel_sum(rc_task_context *tc, void *ctx)
{
    sum_job *j = ctx;
    if (j->hi - j->lo <= 4096) {
        int64_t sum = 0;
        for (uint32_t i = j->lo; i < j->hi; ++i)
            sum += j->v[i];
        j->out = sum;
        return;
    }
    uint32_t mid = j->lo + (j->hi - j->lo) / 2;
    sum_job *l = rc_arena_alloc_type(tc->scratch, sum_job, 1);
    sum_job *r = rc_arena_alloc_type(tc->scratch, sum_job, 1);
    *l = (sum_job) {.v = j->v, .lo = j->lo, .hi = mid};
    *r = (sum_job) {.v = j->v, .lo = mid, .hi = j->hi};

    rc_task lt = rc_scheduler_run(tc->scheduler, parallel_sum, l);
    rc_task rt = rc_scheduler_run(tc->scheduler, parallel_sum, r);
    rc_scheduler_wait(tc->scheduler, lt);
    rc_scheduler_wait(tc->scheduler, rt);
    j->out = l->out + r->out;
}

sum_job top = {.v = data, .lo = 0, .hi = n};
rc_scheduler_wait(s, rc_scheduler_run(s, parallel_sum, &top));
// top.out holds the sum
```

### richc/template/future.h - typed futures

A template header (it lives in `template/`, documented here with the scheduler
it wraps) generating a typed future over a scheduler task. A future is just the
scheduler plus the task handle plus an optional result pointer - no allocation,
no reference counting, and deliberately no separate promise type: the task's
slot is the shared state, and the producer side is a typed write into it.
Control macros: `RC_FUTURE_TYPE` (required) and `RC_FUTURE_NAME` (optional;
default is the type's spelling). Both are undefined again by the header.

Generated for `RC_FUTURE_TYPE int` (all `static inline`):

| API | Description |
|-----|-------------|
| `rc_future_int` | the future value type |
| `rc_scheduler_run_int(s, fn, ctx, out) -> rc_future_int` | submit a task and return a future for its result. `out` is caller-owned result storage of any size, kept alive until `get`; pass NULL to use the slot's inline buffer instead, which requires `sizeof(RC_FUTURE_TYPE) <= RC_TASK_RESULT_SIZE` (checked at run time) |
| `rc_future_int_result(tc) -> int *` | producer side, from inside the task: typed pointer to the result storage (build a struct in place) |
| `rc_future_int_set(tc, v)` | producer side: store a scalar result |
| `rc_future_int_get(f) -> int` | consumer side: wait for the task and return the result (read straight from `*out`, or copied out of the inline buffer, which also releases the slot) |

```c
#define RC_FUTURE_TYPE int
#include "richc/template/future.h"

typedef struct add_args {
    int a;
    int b;
} add_args;

static void add_task(rc_task_context *tc, void *ctx)
{
    add_args *in = ctx;
    rc_future_int_set(tc, in->a + in->b);
}

add_args in = {.a = 20, .b = 22};
int out;
rc_future_int f = rc_scheduler_run_int(s, add_task, &in, &out);  // or out = NULL: inline slot buffer
int sum = rc_future_int_get(f);   // 42
```

A struct result is built in place through the typed pointer instead:

```c
static void make_row(rc_task_context *tc, void *ctx)
{
    row *r = rc_future_row_result(tc);
    // fill *r field by field
}
```

---

## Unit testing

`richc/test.h`. A self-registering unit-test framework: each test macro places
a descriptor pointer into a dedicated linker section and the runner walks that
section, so no registration list is needed. Part of the core library, but only
linked into an executable when something references `rc_test_run`. Supported on
Windows (MSVC, clang, gcc) and Linux (clang, gcc).

| API | Description |
|-----|-------------|
| `RC_TEST(group, name) { ... }` | define a test, labelled `group.name` (bare identifiers); the body is the test function |
| `RC_TEST_SKIP(group, name) { ... }` | registered but not run; reported SKIP |
| `RC_TEST_GROUP_DATA(group) { ... };` | declare the per-group fixture struct |
| `RC_TEST_GROUP_INIT(group, fix) { ... }`<br>`RC_TEST_GROUP_DEINIT(group, fix) { ... }` | run before / after each fixtured test in the group; `fix` names the fixture pointer |
| `RC_TEST_STEP(group, name, fix) { ... }` | a test that receives the fixture |
| `RC_TEST_STEP_SKIP(group, name, fix)` | registered fixtured test, not run |
| `RC_CHECK(a, op, b)` | assert `a op b`; the left operand selects the comparison via `_Generic` (see below) |
| `RC_CHECK_TRUE(a)`<br>`RC_CHECK_FALSE(a)` | assert truthy / falsy |
| `rc_test_run(filter) -> int` | run every test whose group name starts with `filter` (`""` for all); prints per-test lines and a summary, returns the failure count |
| `RC_TEST_MAIN()` | emit a `main` calling `rc_test_run` with the first command-line argument as the filter; place once per test executable |

`RC_CHECK` operand types and operators:

| Operand type | Operators |
|--------------|-----------|
| `bool` | `==` `!=` |
| `int8_t` .. `uint64_t` (all fixed-width integers) | `==` `!=` `<` `>` `<=` `>=` |
| `float`, `double` | `==` `!=` `<` `>` `<=` `>=` and `~=` |
| `rc_str` | `==` `!=` |
| `rc_vec2i`, `rc_vec3i` | `==` `!=` |
| `rc_vec2f`, `rc_vec3f`, `rc_vec4f` | `==` `!=` and `~=` |
| `rc_rational` | `==` `!=` `<` `>` `<=` `>=` |

`~=` is an approximate compare with a fixed epsilon of 0.0001 (for vectors,
every component within epsilon); the vector `==`/`!=` are exact. A failing
assertion prints the file, line, expression, and actual value, then aborts the
current test; the runner records the failure and continues.

```c
RC_TEST(str, basics)
{
    rc_str s = RC_STR("hello world");
    RC_CHECK(s.len, ==, 11u);
    RC_CHECK(rc_vec2f_normalize(rc_vec2f_make(3.0f, 4.0f)), ~=, rc_vec2f_make(0.6f, 0.8f));
}
```

---

## Macros and assertions

`richc/macros.h`. Small preprocessor utilities and the assertion macros used
across the library. `RC_ASSERT` is for *internal* preconditions (programmer
error); untrusted external input gets validated and returns an error instead.
`RC_ASSERT` is an expression, not a statement, so it can sit on the left of a
comma operator (`*(RC_ASSERT(i < v.num), v.data + i)`).

| API | Description |
|-----|-------------|
| `RC_CONCAT(a, b)` | paste two tokens, expanding macros first (`RC_CONCAT(rc_array_, int)` -> `rc_array_int`) |
| `RC_STRINGIFY(x)` | convert a token to a string literal, expanding macros first |
| `RC_INDEX_NONE` | the `uint32_t` "not found" / "invalid" index sentinel (`UINT32_MAX`) |
| `RC_ASSERT(cond)` | debug-only assertion: a false `cond` triggers a debug break; under `NDEBUG` `cond` is evaluated and discarded (so assertion-only variables do not warn) |
| `RC_PANIC(cond)` | always-active assertion: traps in all builds. For unrecoverable invariants such as out-of-memory |
| `RC_UNREACHABLE()` | marks a provably dead path (e.g. the `default` of a switch over a validated closed set): traps in debug, and in all builds emits the compiler's unreachable hint, so the switch is treated as exhaustive with no dummy return needed |
