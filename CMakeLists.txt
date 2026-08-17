cmake_minimum_required(VERSION 3.23)
project(Mini_Cloud
    VERSION 0.1.0
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network)

qt_standard_project_setup()

add_subdirectory(ProtocolCommon)
add_subdirectory(MainApplication)
add_subdirectory(ServerApplication)
