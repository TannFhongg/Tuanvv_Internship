cmake_minimum_required(VERSION 3.20)

project(
    TuanVV_Internship
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(TUANVV_ENABLE_WARNINGS "Enable the shared compiler warning policy" ON)
option(TUANVV_ENABLE_SANITIZERS "Enable Address and Undefined Behavior Sanitizers" OFF)

add_library(tuanvv_build_options INTERFACE)
target_compile_features(tuanvv_build_options INTERFACE cxx_std_20)

if(TUANVV_ENABLE_WARNINGS)
    include(cmake/CompilerWarnings.cmake)
    tuanvv_enable_warnings(tuanvv_build_options)
endif()

if(TUANVV_ENABLE_SANITIZERS)
    include(cmake/Sanitizers.cmake)
    tuanvv_enable_sanitizers(tuanvv_build_options)
endif()

include(CTest)
