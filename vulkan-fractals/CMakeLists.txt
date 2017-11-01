cmake_minimum_required(VERSION 3.8)
project(vulkan-fractals)

add_subdirectory(fmt)

set(CMAKE_CXX_STANDARD 14)

include_directories(inc)
link_directories(lib)

set(SOURCE_FILES main.cpp Application.cpp Application.h util.h util.cpp)
add_executable(vulkan-fractals ${SOURCE_FILES})

target_link_libraries(vulkan-fractals vulkan-1 glfw3 fmt)
