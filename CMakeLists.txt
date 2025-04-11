cmake_minimum_required(VERSION 3.14)

project(script_parallel_exec CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CXX_EXTENSIONS NO)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_executable(doubles-with-templates with-templates/main_doubles.cpp)
add_executable(ints-with-templates with-templates/main_ints.cpp)

add_executable(doubles-no-templates no-templates/main_doubles.cpp)
add_executable(ints-no-templates no-templates/main_ints.cpp)

if (WIN32)
	add_library(windows-affinity-run-params STATIC
		windows-affinity/run_params.hpp
		windows-affinity/run_params.cpp)

	add_executable(doubles-windows-affinity windows-affinity/main_doubles.cpp)
	target_link_libraries(doubles-windows-affinity PRIVATE
		windows-affinity-run-params)

	add_executable(ints-windows-affinity windows-affinity/main_ints.cpp)
	target_link_libraries(ints-windows-affinity PRIVATE
		windows-affinity-run-params)

	# Нужна статическая C++ runtime-библиотека.
	set_property(TARGET doubles-with-templates PROPERTY
		MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
	set_property(TARGET ints-with-templates PROPERTY
		MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

	set_property(TARGET doubles-no-templates PROPERTY
		MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
	set_property(TARGET ints-no-templates PROPERTY
		MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

	set_property(TARGET windows-affinity-run-params PROPERTY
		MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

	set_property(TARGET doubles-windows-affinity PROPERTY
		MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
	set_property(TARGET ints-windows-affinity PROPERTY
		MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()
