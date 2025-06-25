add_subdirectory("app")
add_subdirectory("math")
add_subdirectory("test")

target_sources("richc" PRIVATE
    "arena.c"
    "bitset.c"
    "file.c"
    "str.c"
)
