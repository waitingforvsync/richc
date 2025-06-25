add_subdirectory("app")
add_subdirectory("gfx")
add_subdirectory("math")
add_subdirectory("templates")
add_subdirectory("test")

target_sources("richc" PUBLIC
    "arena.h"
    "bitset.h"
    "byte_array.h"
    "defines.h"
    "file.h"
    "str.h"
)
