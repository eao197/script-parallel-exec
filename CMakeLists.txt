cmake_minimum_required(VERSION 3.14)

project(script_parallel_exec CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CXX_EXTENSIONS NO)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_executable(doubles main_doubles.cpp)

add_executable(ints main_ints.cpp)

