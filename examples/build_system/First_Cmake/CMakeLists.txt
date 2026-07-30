cmake_minimum_required(VERSION 3.24)

project(
    windowUI
    VERSION 1.0.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(project_warnings INTERFACE)

target_compile_options(
    project_warnings
    INTERFACE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic;-Wshadow>
        $<$<CXX_COMPILER_ID:MSVC>:/W4>
)

include(FetchContent)

set(RAYLIB_VERSION 4.5.0)

FetchContent_Declare(
    raylib
    URL
        https://github.com/raysan5/raylib/archive/refs/tags/${RAYLIB_VERSION}.tar.gz
)

set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(raylib)

add_executable(
    app
    main.cpp
)

target_link_libraries(
    app
    PRIVATE
        project_warnings
        raylib
)