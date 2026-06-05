# ---- core test executable ----

add_executable(richc_core_test
    main.c
    test_arena.c
    test_array.c
    test_bitset.c
    test_bitset_foreach.c
    test_box2f.c
    test_box2i.c
    test_bytes.c
    test_file.c
    test_hash.c
    test_hash_trie.c
    test_lower_bound.c
    test_mat22f.c
    test_mat23f.c
    test_mat33f.c
    test_mat34f.c
    test_mat44f.c
    test_max_element.c
    test_min_element.c
    test_mstr.c
    test_ops.c
    test_pool.c
    test_pool_foreach.c
    test_quatf.c
    test_rational.c
    test_solve.c
    test_sort.c
    test_str.c
    test_upper_bound.c
    test_vec2f.c
    test_vec2i.c
    test_vec3f.c
    test_vec3i.c
    test_vec4f.c
    test_zip_inflate.c
)

target_link_libraries(richc_core_test PRIVATE richc)

if(MSVC)
    target_compile_options(richc_core_test PRIVATE /W3)
else()
    target_compile_options(richc_core_test PRIVATE -Wall -Wextra)
endif()

# ---- run tests as a post-build step ----

# The tests run with the working directory set to the binary's own directory,
# so file tests address scratch files by a plain relative name (written into the
# build tree, not a hard-coded /tmp) and bundled read-only fixtures by a relative
# "data/..." path. The fixtures live in test/data/ and are staged next to the
# binary first so that relative path resolves in every generator (including
# multi-config, where the binary sits in a per-config subdirectory).
add_custom_command(
    TARGET richc_core_test POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/data $<TARGET_FILE_DIR:richc_core_test>/data
    COMMAND $<TARGET_FILE:richc_core_test>
    WORKING_DIRECTORY $<TARGET_FILE_DIR:richc_core_test>
    COMMENT "Running richc core tests"
    VERBATIM
)
