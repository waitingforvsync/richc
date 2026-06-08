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
    src/app/app_glfw.c
    src/image/image.c
    src/image/image_atlas.c
    src/image/image_pack.c
    src/image/image_png.c
    include/richc/app/app.h
    include/richc/app/keys.h
    include/richc/image/array/image.h
    include/richc/image/image.h
    include/richc/image/image_atlas.h
    include/richc/image/image_pack.h
    include/richc/image/image_png.h
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

# ---- app test executable ----

add_subdirectory(test)

# ---- example application (development sandbox) ----

add_executable(richc_app_example
    example/main.c
)

# The example links glad directly so the sandbox can issue raw GL calls (e.g.
# clearing the screen) before the gfx abstraction exists. glad's loader state is
# shared with richc_app, which calls gladLoadGL during rc_app_init.
target_link_libraries(richc_app_example PRIVATE richc_app glad)

target_compile_definitions(richc_app_example PRIVATE _CRT_SECURE_NO_WARNINGS _DEFAULT_SOURCE)

if(MSVC)
    target_compile_options(richc_app_example PRIVATE /W3)
else()
    target_compile_options(richc_app_example PRIVATE -Wall -Wextra)
endif()
