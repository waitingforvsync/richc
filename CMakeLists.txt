cmake_minimum_required(VERSION 3.31)
project("richc")

set(CMAKE_C_STANDARD 17)

add_library("richc" STATIC)
set_target_properties("richc" PROPERTIES DEBUG_POSTFIX "_d")

target_compile_options("richc" PRIVATE
	"$<$<C_COMPILER_ID:MSVC>:/W4;/WX;/wd4100;/wd4820;/wd4189;/wd4206;/wd4101>"
	"$<$<C_COMPILER_ID:Clang,GNU>:-Wall;-Wextra;-Wpedantic;-Werror;-Wno-unused-but-set-variable;-Wno-unused-parameter;-Wno-unused-variable;-Wno-unused-function>"
)

target_compile_definitions("richc" PRIVATE
	"$<$<PLATFORM_ID:Windows>:RICHC_PLATFORM_WINDOWS=1>"
	"$<$<PLATFORM_ID:Darwin>:RICHC_PLATFORM_MACOS=1>"
	"$<$<PLATFORM_ID:Linux>:RICHC_PLATFORM_LINUX=1>"
)

add_subdirectory("include")
add_subdirectory("src")

set(BUILD_TESTS ON CACHE BOOL "Build and run richc tests")
if(BUILD_TESTS)
    add_subdirectory("test")
endif()

set(BUILD_SAMPLES ON CACHE BOOL "Build richc samples")
if(BUILD_SAMPLES)
	add_subdirectory("samples")
endif()
