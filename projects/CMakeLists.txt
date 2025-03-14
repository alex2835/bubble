cmake_minimum_required(VERSION 3.12)

if(WIN32)
	if(MSVC)
		SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} /WX")
	endif()
elseif(APPLE)
	set(CMAKE_C_COMPILER "clang")
	set(CMAKE_CXX_COMPILER "clang++")
endif()
set(CMAKE_CXX_STANDARD 23)

add_subdirectory(editor)
add_subdirectory(engine)