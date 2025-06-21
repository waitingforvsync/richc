cmake_minimum_required(VERSION 3.31)
project("richc")

set(CMAKE_C_STANDARD 11)

add_library("richc" STATIC)
set_target_properties("richc" PROPERTIES DEBUG_POSTFIX "_d")
if(MSVC)
	target_compile_options("richc" PRIVATE /W4 /WX /wd4100 /wd4820 /wd4189 /wd4206 /wd4101)
else()
	target_compile_options("richc" PRIVATE -Wall -Wextra -Wpedantic -Werror -Wno-unused-but-set-variable -Wno-unused-parameter -Wno-unused-variable -Wno-unused-function)
endif()

add_subdirectory("include")
add_subdirectory("src")

set(WITH_TESTS ON CACHE BOOL "Run richc tests")
if(WITH_TESTS)
    add_subdirectory("test")
endif()
