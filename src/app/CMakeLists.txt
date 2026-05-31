# ---- vendored dependencies (extern/) ----

# GLFW (window creation, input, GL context) - build only the library, statically.
set(GLFW_LIBRARY_TYPE   STATIC CACHE STRING "" FORCE)
set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(extern/glfw)

# glad - OpenGL 3.3 core loader, pre-generated (glad 2.0.8) and vendored as
# source (see extern/glad/README.md).
add_subdirectory(extern/glad)

# ---- richc_app static library ----

add_library(richc_app STATIC
    src/app/app.c
    include/richc/app/app.h
)

target_include_directories(richc_app PUBLIC include)

target_link_libraries(richc_app
    PUBLIC  richc
    PRIVATE glfw glad
)

target_compile_definitions(richc_app PRIVATE _CRT_SECURE_NO_WARNINGS _DEFAULT_SOURCE)

if(MSVC)
    target_compile_options(richc_app PRIVATE /W3)
else()
    target_compile_options(richc_app PRIVATE -Wall -Wextra)
endif()

# ---- example application (development sandbox) ----

add_executable(richc_app_example
    example/main.c
)

target_link_libraries(richc_app_example PRIVATE richc_app)

target_compile_definitions(richc_app_example PRIVATE _CRT_SECURE_NO_WARNINGS _DEFAULT_SOURCE)

if(MSVC)
    target_compile_options(richc_app_example PRIVATE /W3)
else()
    target_compile_options(richc_app_example PRIVATE -Wall -Wextra)
endif()
