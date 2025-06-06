cmake_minimum_required(VERSION 3.30)

project(MWL VERSION 0.1.0 LANGUAGES CXX C)

option(MWL_BUILD_SHARED_LIBS "Build shared libraries" OFF)
option(MWL_BUILD_EXAMPLES "Build MWL example programs" ON)
option(MWL_INCLUDE_WAYLAND "Include support for Wayland" ON)
option(MWL_DISABLE_TRAPS "Prevent MWL from using debug traps" OFF)

if (MWL_BUILD_SHARED_LIBS)
    set(MWL_LIBRARY_TYPE "SHARED")
else()
    set(MWL_LIBRARY_TYPE "STATIC")
endif()

if(MSVC)
    string(REGEX REPLACE "/W[1-3]" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
else()
    add_compile_options(-Wall -Wextra -pedantic -Werror)
endif()

if (UNIX AND NOT APPLE)
    set(MWL_PLATFORM_LINUX ON)
elseif(WIN32)
    set(MWL_PLATFORM_WINDOWS ON)
else()
    message(FATAL_ERROR "Unsupported Platform!")
endif()

if (MWL_INCLUDE_WAYLAND)
    message("Including Wayland support")
endif()

add_subdirectory(src)

if (MWL_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
