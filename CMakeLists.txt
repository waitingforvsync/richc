cmake_minimum_required(VERSION 3.21)
project(richc C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# When ON, build only the core static library: skip the app layer (which pulls in
# GLFW/glad) and richc's own test executables. Lets a consumer add_subdirectory()
# richc and link `richc` without dragging in windowing dependencies or tests.
option(RICHC_CORE_ONLY "Build only the richc core library" OFF)

add_subdirectory(src)
