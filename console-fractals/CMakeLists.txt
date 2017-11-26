cmake_minimum_required(VERSION 3.0)
project(console-fractals)

set(CMAKE_CXX_STANDARD 14)

set(SOURCE_FILES main.cpp Display.cpp Display.h Dimension.h)
add_executable(console-fractals ${SOURCE_FILES})
