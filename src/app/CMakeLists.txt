# ---- vendored dependencies (extern/) ----

# GLFW (window creation, input, GL context) - build only the library, statically.
set(GLFW_LIBRARY_TYPE   STATIC CACHE STRING "" FORCE)
set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL        OFF CACHE BOOL "" FORCE)
add_subdirectory(extern/glfw)

# glad - OpenGL 3.3 core loader, pre-generated (glad 2.0.8) and vendored as
# source (see extern/glad/README.md).
add_subdirectory(extern/glad)

# ---- richc_app static library ----

# gfx backend, selected at compile time (which backend sources are built).
set(RICHC_GFX_BACKEND "gl33" CACHE STRING "gfx backend (gl33)")
if(RICHC_GFX_BACKEND STREQUAL "gl33")
    set(RICHC_GFX_BACKEND_SOURCES
        src/gfx/gl/gfx_gl33.c
        src/gfx/gl/gfx_gl33_convert.c
        src/gfx/gl/gfx_gl33_internal.h
    )
    set(RICHC_GFX_BACKEND_DEFINE RC_GFX_BACKEND_GL33)
else()
    message(FATAL_ERROR "unknown RICHC_GFX_BACKEND: ${RICHC_GFX_BACKEND}")
endif()

add_library(richc_app STATIC
    src/app/app_glfw.c
    src/font/font.c
    src/font/font_atlas.c
    src/font/font_sdf.c
    src/font/font_ttf.c
    src/gfx/gfx.c
    src/gfx/gfx_color.c
    src/gfx/gfx_encoder.c
    src/image/image.c
    src/image/image_atlas.c
    src/image/image_pack.c
    src/image/image_png.c
    include/richc/app/app.h
    include/richc/app/keys.h
    include/richc/font/font.h
    include/richc/font/font_atlas.h
    include/richc/gfx/bindings.h
    include/richc/gfx/buffer.h
    include/richc/gfx/color.h
    include/richc/gfx/encoder.h
    include/richc/gfx/gfx.h
    include/richc/gfx/pass.h
    include/richc/gfx/pipeline.h
    include/richc/gfx/shader.h
    include/richc/gfx/texture.h
    include/richc/image/image.h
    include/richc/image/image_atlas.h
    include/richc/image/image_pack.h
    include/richc/image/image_png.h
    include/richc/image/array/image.h
    src/font/font_sdf.h
    src/font/font_ttf.h
    src/gfx/gfx_internal.h
    ${RICHC_GFX_BACKEND_SOURCES}
)

target_include_directories(richc_app PUBLIC include)

target_link_libraries(richc_app
    PUBLIC  richc
    PRIVATE glfw glad
)

target_compile_definitions(richc_app PRIVATE
    _CRT_SECURE_NO_WARNINGS _DEFAULT_SOURCE ${RICHC_GFX_BACKEND_DEFINE})

if(MSVC)
    target_compile_options(richc_app PRIVATE /W3)
else()
    target_compile_options(richc_app PRIVATE -Wall -Wextra)
endif()

# ---- app test executable ----

add_subdirectory(test)

# ---- example applications (development sandbox) ----

add_subdirectory(example)
