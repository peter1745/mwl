cmake_minimum_required(VERSION 3.30)

function(register_example name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE mwl)
endfunction()

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_COMPILE_WARNING_AS_ERROR ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Example Executables
register_example(simple_window)
register_example(draw_screen_buffer)
register_example(fullscreen)
register_example(input)
