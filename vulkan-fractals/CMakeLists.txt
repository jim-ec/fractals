cmake_minimum_required(VERSION 3.8)
project(vulkan-fractals)

add_subdirectory(fmt)
add_subdirectory(glfw-3.2.1)

set(CMAKE_CXX_STANDARD 14)

include_directories(inc)
if (MSVC)
    link_directories(lib-win32)
elseif (UNIX)
    SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lvulkan")
endif ()

set(SOURCE_FILES main.cpp Application.cpp Application.h util.h util.cpp)
add_executable(vulkan-fractals ${SOURCE_FILES})

if (MSVC)
    target_link_libraries(vulkan-fractals vulkan-1 glfw fmt)
elseif (UNIX)
    target_link_libraries(vulkan-fractals glfw fmt)
endif ()
