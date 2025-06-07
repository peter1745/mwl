cmake_minimum_required(VERSION 3.30)

# ECM
find_package(ECM REQUIRED NO_MODULE)
find_package(PkgConfig)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_COMPILE_WARNING_AS_ERROR ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(mwl ${MWL_LIBRARY_TYPE}
    mwl.cpp)

target_include_directories(mwl PUBLIC ${CMAKE_SOURCE_DIR}/include/)

if (MWL_DISABLE_TRAPS)
    target_compile_definitions(mwl PRIVATE MWL_DISABLE_TRAPS)
endif()

if (MWL_PLATFORM_LINUX)
    target_compile_definitions(mwl PUBLIC MWL_PLATFORM_LINUX)
    target_link_libraries(mwl PRIVATE stdc++exp)
elseif(MWL_PLATFORM_WINDOWS)
    target_compile_definitions(mwl PUBLIC MWL_PLATFORM_WINDOWS)
endif()

if (MWL_INCLUDE_WAYLAND)
    include(FindWayland)
    include(FindWaylandScanner)

    pkg_check_modules(XKBCommon REQUIRED xkbcommon)

    target_sources(mwl PRIVATE mwl_wayland.cpp)
    target_compile_definitions(mwl PUBLIC MWL_INCLUDE_WAYLAND)
    target_link_libraries(mwl PRIVATE ${Wayland_LIBRARIES} ${XKBCommon_LIBRARIES})

    # Generate Wayland Protocol C files
    # NOTE: Potentially look into using hyprwayland-scanner instead
    # since it generates C++ bindings instead of C bindings
    if (NOT WaylandScanner_FOUND)
        message(FATAL_ERROR "FindWaylandScanner failed to find wayland-scanner.")
    else()
        ecm_add_wayland_client_protocol(mwl
            PROTOCOL /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml
            BASENAME xdg-shell)

        target_include_directories(mwl PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
    endif()
endif()

