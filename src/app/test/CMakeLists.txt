# ---- app test executable ----

add_executable(richc_app_test
    main.c
    test_image.c
)

target_link_libraries(richc_app_test PRIVATE richc_app)

if(MSVC)
    target_compile_options(richc_app_test PRIVATE /W3)
else()
    target_compile_options(richc_app_test PRIVATE -Wall -Wextra)
endif()

# ---- run tests as a post-build step ----

add_custom_command(
    TARGET richc_app_test POST_BUILD
    COMMAND $<TARGET_FILE:richc_app_test>
    COMMENT "Running richc app tests"
    VERBATIM
)
