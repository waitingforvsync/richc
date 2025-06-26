find_package("OpenGL" REQUIRED)
find_package("GLEW" REQUIRED)
find_package("SDL2" REQUIRED)

target_sources("richc" PRIVATE
    "app.c"
)

target_link_libraries("richc" PRIVATE
    "SDL2::SDL2"
    "SDL2::SDL2main"
    "GLEW::GLEW"
    "OpenGL::GL"
)
