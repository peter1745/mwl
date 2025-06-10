cmake_minimum_required(VERSION 3.30)

link_libraries(mwl)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_COMPILE_WARNING_AS_ERROR ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Example Executables
add_executable(simple_window simple_window.cpp)
add_executable(draw_screen_buffer draw_screen_buffer.cpp)
add_executable(fullscreen fullscreen.cpp)
add_executable(input input.cpp)
