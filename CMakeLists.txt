cmake_minimum_required(VERSION 3.12)
project(bubble)

find_package(OpenGL REQUIRED)
set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CMAKE_COMMAND} -E time")

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

option(ASAN "Adders sanitazer" OFF)
if (ASAN)
	message(WARNING "Adders sanitazer is ON")
	SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} /MT -fsanitize=address")
else()
	message(WARNING "Adders sanitazer is OFF")
endif()

add_subdirectory(deps)
add_subdirectory(projects)
