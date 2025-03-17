cmake_minimum_required(VERSION 3.12)

set(CMAKE_CXX_STANDARD 23)
if(WIN32)
	if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
		SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS}\
		    -Werror\
			-Wno-format-security\
			-Wno-unused-variable\
			-Wno-misleading-indentation")
	elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
		SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} /WX")
	endif()
elseif(APPLE)
	set(CMAKE_C_COMPILER "clang")
	set	(CMAKE_CXX_COMPILER "clang++")
endif()

add_subdirectory(editor)
add_subdirectory(engine)