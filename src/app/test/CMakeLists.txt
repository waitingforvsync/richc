# ---- app test executable ----

add_executable(richc_app_test
    main.c
    test_image.c
    test_image_png.c
)

target_link_libraries(richc_app_test PRIVATE richc_app)

if(MSVC)
    target_compile_options(richc_app_test PRIVATE /W3)
else()
    target_compile_options(richc_app_test PRIVATE -Wall -Wextra)
endif()

# ---- run tests as a post-build step ----

# Stage the binary test fixtures (data/) next to the test binary first, then run
# with that directory as the working dir so the tests resolve "data/..." paths.
add_custom_command(
    TARGET richc_app_test POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/data $<TARGET_FILE_DIR:richc_app_test>/data
    COMMAND $<TARGET_FILE:richc_app_test>
    WORKING_DIRECTORY $<TARGET_FILE_DIR:richc_app_test>
    COMMENT "Running richc app tests"
    VERBATIM
)
