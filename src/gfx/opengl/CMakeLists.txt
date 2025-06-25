find_package("OpenGL" REQUIRED)

target_link_libraries("richc" PRIVATE
    "OpenGL::GL"
)

target_sources("richc" PRIVATE
    "opengl.c"
    "opengl.h"
)
