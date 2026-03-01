cmake_minimum_required(VERSION 3.21)
project(richc C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# ---- static library ----

add_library(richc STATIC
    src/arena.c
    src/str.c
    src/mstr.c
    src/file.c
    src/math/mat44f.c
    src/math/rational.c
    src/math/bigint.c
    include/richc/arena.h
    include/richc/bytes.h
    include/richc/debug.h
    include/richc/file.h
    include/richc/hash.h
    include/richc/mstr.h
    include/richc/platform.h
    include/richc/str.h
    include/richc/template_util.h
    include/richc/math/aabb2f.h
    include/richc/math/aabb2i.h
    include/richc/math/mat22f.h
    include/richc/math/mat23f.h
    include/richc/math/mat33f.h
    include/richc/math/mat34f.h
    include/richc/math/mat44f.h
    include/richc/math/math.h
    include/richc/math/quatf.h
    include/richc/math/bigint.h
    include/richc/math/rational.h
    include/richc/math/vec2f.h
    include/richc/math/vec2i.h
    include/richc/math/vec3i.h
    include/richc/math/vec3f.h
    include/richc/math/vec4f.h
    include/richc/template/accumulate.h
    include/richc/template/all_of.h
    include/richc/template/any_of.h
    include/richc/template/array.h
    include/richc/template/find.h
    include/richc/template/hash_map.h
    include/richc/template/hash_set.h
    include/richc/template/hash_trie.h
    include/richc/template/lower_bound.h
    include/richc/template/max_element.h
    include/richc/template/min_element.h
    include/richc/template/mismatch.h
    include/richc/template/none_of.h
    include/richc/template/remove.h
    include/richc/template/rotate.h
    include/richc/template/sort.h
    include/richc/template/transform.h
    include/richc/template/upper_bound.h
)

target_include_directories(richc PUBLIC include)

target_compile_options(richc PRIVATE -Wall -Wextra)

# ---- test executable ----

add_executable(richc_test test/test.c)

target_link_libraries(richc_test PRIVATE richc)

target_compile_options(richc_test PRIVATE -Wall -Wextra)

# ---- run tests as a post-build step ----

add_custom_command(
    TARGET richc_test POST_BUILD
    COMMAND $<TARGET_FILE:richc_test>
    COMMENT "Running richc tests..."
    VERBATIM
)
