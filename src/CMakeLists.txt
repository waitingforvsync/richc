target_include_directories("richc" PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

add_subdirectory("app")
add_subdirectory("gfx")
add_subdirectory("math")
add_subdirectory("test")

target_sources("richc" PRIVATE
    "arena.c"
    "bitset.c"
    "file.c"
    "str.c"
)
