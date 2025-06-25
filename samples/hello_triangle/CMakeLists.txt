add_executable("hello_triangle")
target_link_libraries("hello_triangle" PRIVATE "richc")

target_sources("hello_triangle" PRIVATE
    "main.c"
)
