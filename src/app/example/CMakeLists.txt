foreach(example cubes hellotriangle text)
    add_executable(richc_example_${example}
        ${example}/main.c
    )

    target_link_libraries(richc_example_${example} PRIVATE richc_app)

    target_compile_definitions(richc_example_${example} PRIVATE _CRT_SECURE_NO_WARNINGS _DEFAULT_SOURCE)

    if(MSVC)
        target_compile_options(richc_example_${example} PRIVATE /W3)
    else()
        target_compile_options(richc_example_${example} PRIVATE -Wall -Wextra)
    endif()
endforeach()

# The text example loads the Roboto test fixture at runtime; stage it next to
# the binary so "data/fonts/..." resolves when run from the binary's directory.
add_custom_command(
    TARGET richc_example_text POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
            $<TARGET_FILE_DIR:richc_example_text>/data/fonts
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/../test/data/fonts/Roboto-Regular.ttf
            $<TARGET_FILE_DIR:richc_example_text>/data/fonts/Roboto-Regular.ttf
    VERBATIM
)
