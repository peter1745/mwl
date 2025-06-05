cmake_minimum_required(VERSION 3.30)

project(MWL VERSION 0.1.0 LANGUAGES CXX C)

option(MWL_BUILD_SHARED_LIBS "Build shared libraries" OFF)
option(MWL_BUILD_EXAMPLES "Build MWL example programs" ON)
option(MWL_INCLUDE_WAYLAND "Include support for Wayland" ON)

if (MWL_BUILD_SHARED_LIBS)
    set(MWL_LIBRARY_TYPE "SHARED")
else()
    set(MWL_LIBRARY_TYPE "STATIC")
endif()

if (MWL_INCLUDE_WAYLAND)
    message("Including Wayland support")
endif()

add_subdirectory(src)

if (MWL_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
