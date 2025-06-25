find_package("X11" REQUIRED)
find_package("OpenGL" REQUIRED COMPONENTS "GLX")

target_link_libraries("richc" PRIVATE
    "${X11_LIBRARIES}"
    "OpenGL::GLX"
)

target_sources("richc" PRIVATE
    "app.c"
    "opengl.c"
    "opengl.h"
)
