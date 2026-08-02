# ---- core static library ----

add_library(richc STATIC
    src/arena.c
    src/bitset.c
    src/file.c
    src/hash.c
    src/math/mat44f.c
    src/math/quatf.c
    src/math/rational.c
    src/math/solve.c
    src/mstr.c
    src/random.c
    src/rect_pack.c
    src/str.c
    src/test.c
    src/thread/cond.c
    src/thread/mutex.c
    src/thread/rwlock.c
    src/thread/scheduler.c
    src/thread/semaphore.c
    src/thread/thread.c
    src/thread/tls.c
    src/time.c
    src/zip/inflate.c
    include/richc/arena.h
    include/richc/array/f32.h
    include/richc/array/f64.h
    include/richc/array/i16.h
    include/richc/array/i32.h
    include/richc/array/i64.h
    include/richc/array/i8.h
    include/richc/array/mstr.h
    include/richc/array/str.h
    include/richc/array/u16.h
    include/richc/array/u32.h
    include/richc/array/u64.h
    include/richc/array/u8.h
    include/richc/bitset.h
    include/richc/bytes.h
    include/richc/file.h
    include/richc/hash.h
    include/richc/macros.h
    include/richc/math/array/box2f.h
    include/richc/math/array/box2i.h
    include/richc/math/array/mat22f.h
    include/richc/math/array/mat23f.h
    include/richc/math/array/mat33f.h
    include/richc/math/array/mat34f.h
    include/richc/math/array/mat44f.h
    include/richc/math/array/quatf.h
    include/richc/math/array/rational.h
    include/richc/math/array/vec2f.h
    include/richc/math/array/vec2i.h
    include/richc/math/array/vec3f.h
    include/richc/math/array/vec3i.h
    include/richc/math/array/vec4f.h
    include/richc/math/box2f.h
    include/richc/math/box2i.h
    include/richc/math/mat22f.h
    include/richc/math/mat23f.h
    include/richc/math/mat33f.h
    include/richc/math/mat34f.h
    include/richc/math/mat44f.h
    include/richc/math/quatf.h
    include/richc/math/rational.h
    include/richc/math/solve.h
    include/richc/math/vec2f.h
    include/richc/math/vec2i.h
    include/richc/math/vec3f.h
    include/richc/math/vec3i.h
    include/richc/math/vec4f.h
    include/richc/mstr.h
    include/richc/ops.h
    include/richc/random.h
    include/richc/rect_pack.h
    include/richc/str.h
    include/richc/template/array.h
    include/richc/template/algorithm/bitset_foreach.h
    include/richc/template/future.h
    include/richc/template/hash_trie.h
    include/richc/template/algorithm/lower_bound.h
    include/richc/template/algorithm/max_element.h
    include/richc/template/algorithm/min_element.h
    include/richc/template/pool.h
    include/richc/template/algorithm/pool_foreach.h
    include/richc/template/algorithm/sort.h
    include/richc/template/algorithm/upper_bound.h
    include/richc/test.h
    include/richc/thread/atomic.h
    include/richc/thread/cond.h
    include/richc/thread/mutex.h
    include/richc/thread/rwlock.h
    include/richc/thread/scheduler.h
    include/richc/thread/semaphore.h
    include/richc/thread/spinlock.h
    include/richc/thread/thread.h
    include/richc/thread/tls.h
    include/richc/time.h
    include/richc/zip/inflate.h
)

target_include_directories(richc PUBLIC include)

# Suppress MSVC CRT deprecation warnings for standard POSIX functions (fopen etc.)
# when building with clang on Windows against the MSVC runtime headers.
# _DEFAULT_SOURCE is PUBLIC: the thread/ headers embed real pthread types, whose
# visibility on glibc depends on this feature-test macro being set (on the
# command line, before any libc header) in every translation unit that includes
# them - consumers included.
target_compile_definitions(richc
    PRIVATE _CRT_SECURE_NO_WARNINGS
    PUBLIC  _DEFAULT_SOURCE)

if(MSVC)
    target_compile_options(richc PRIVATE /W3)
else()
    target_compile_options(richc PRIVATE -Wall -Wextra)
endif()

find_package(Threads REQUIRED)
target_link_libraries(richc PRIVATE m Threads::Threads)

# ---- core test executable ----

if(NOT RICHC_CORE_ONLY)
    add_subdirectory(test)
endif()
