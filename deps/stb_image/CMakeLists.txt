cmake_minimum_required(VERSION 3.12)

set(SOURCES
	stb_image.cpp
	stb_image.h
)
add_library(stb_image ${SOURCES})

target_include_directories(stb_image PUBLIC 
	${CMAKE_CURRENT_SOURCE_DIR}
)