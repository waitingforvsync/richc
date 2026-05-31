# glad - pre-generated OpenGL 3.3 core loader, vendored as source (see README.md).
# Built as a plain static library; no Python or network at build time. The loader
# links no GL library - it resolves entry points through a caller-supplied loader
# (e.g. glfwGetProcAddress).

add_library(glad STATIC
    src/gl.c
    include/glad/gl.h
    include/KHR/khrplatform.h
)

target_include_directories(glad PUBLIC include)
