cmake_minimum_required(VERSION 3.14)

project(script_parallel_exec CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CXX_EXTENSIONS NO)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_executable(doubles-with-templates with-templates/main_doubles.cpp)

add_executable(ints-with-templates with-templates/main_ints.cpp)

add_executable(doubles-no-templates no-templates/main_doubles.cpp)

add_executable(ints-no-templates no-templates/main_ints.cpp)

if (WIN32)
	# Нужна статическая C++ runtime-библиотека.
	set_property(TARGET doubles-with-templates PROPERTY
		MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
	set_property(TARGET ints-with-templates PROPERTY
		MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
	set_property(TARGET doubles-no-templates PROPERTY
		MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
	set_property(TARGET ints-no-templates PROPERTY
		MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()
