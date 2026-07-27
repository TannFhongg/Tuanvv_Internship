cmake_minimum_required(VERSION 3.24)
project(hello_raylib)

set(CMAKE_CXX_STANDARD 11)

# Dependencies
include(FetchContent)
set(RAYLIB_VERSION 4.5.0)


#The code above says we want a package we call raylib.
#If it isn’t available on this machine, then download it from the specified URL. 
#The FIND_PACKAGE_ARGS is passed to find_package() when it’s looking for raylib. We want it to find the EXACT version we want (i.e., ${RAYLIB_VERSION}).
FetchContent_Declare(
    raylib
    URL https://github.com/raysan5/raylib/archive/refs/tags/${RAYLIB_VERSION}.tar.gz
    FIND_PACKAGE_ARGS ${RAYLIB_VERSION} EXACT 
)
#Raylib has a BUILD_EXAMPLES option. We don’t need the examples, and building them would be a waste of time.
#So, we disable it using the line above
set(BUILD_EXAMPLES OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(raylib)

set(SOURCES_FILE
main.cpp
)
add_executable(${PROJECT_NAME} ${SOURCES_FILE})
target_link_libraries(${PROJECT_NAME} raylib)

