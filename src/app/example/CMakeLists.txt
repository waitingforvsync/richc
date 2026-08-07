foreach(example cubes hellotriangle)
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
