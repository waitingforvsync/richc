find_package("X11" REQUIRED)

target_link_libraries("richc" PRIVATE
    "${X11_LIBRARIES}"
)

target_sources("richc" PRIVATE
    "app.c"
)
