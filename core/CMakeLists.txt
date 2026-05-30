# ---- core static library ----

add_library(richc STATIC
    src/arena.c
    src/file.c
    src/hash.c
    src/mstr.c
    src/str.c
    src/test.c
    include/richc/arena.h
    include/richc/bytes.h
    include/richc/file.h
    include/richc/hash.h
    include/richc/macros.h
    include/richc/math/vec2i.h
    include/richc/math/vec3i.h
    include/richc/mstr.h
    include/richc/ops.h
    include/richc/str.h
    include/richc/template/array.h
    include/richc/test.h
)

target_include_directories(richc PUBLIC include)

# Suppress MSVC CRT deprecation warnings for standard POSIX functions (fopen etc.)
# when building with clang on Windows against the MSVC runtime headers.
target_compile_definitions(richc PRIVATE _CRT_SECURE_NO_WARNINGS _DEFAULT_SOURCE)

if(MSVC)
    target_compile_options(richc PRIVATE /W3)
else()
    target_compile_options(richc PRIVATE -Wall -Wextra)
endif()

target_link_libraries(richc PRIVATE m)

# ---- core test executable ----

add_executable(richc_core_test
    test/test_arena.c
    test/test_array.c
    test/test_bytes.c
    test/test_file.c
    test/test_hash.c
    test/test_mstr.c
    test/test_ops.c
    test/test_str.c
    test/test_vec2i.c
    test/test_vec3i.c
)

target_link_libraries(richc_core_test PRIVATE richc)

if(MSVC)
    target_compile_options(richc_core_test PRIVATE /W3)
else()
    target_compile_options(richc_core_test PRIVATE -Wall -Wextra)
endif()

# ---- run tests as a post-build step ----

add_custom_command(
    TARGET richc_core_test POST_BUILD
    COMMAND $<TARGET_FILE:richc_core_test>
    COMMENT "Running richc core tests"
    VERBATIM
)
