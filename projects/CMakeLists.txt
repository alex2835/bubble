cmake_minimum_required(VERSION 3.12)

set(CMAKE_CXX_STANDARD 23)
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS}\
	    -Werror\
		-Wno-format-security\
		-Wno-unused-variable\
		-Wno-misleading-indentation")
	if(APPLE)
		SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -stdlib=libc++")
	endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
	SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} /WX")
endif()

add_subdirectory(editor)
add_subdirectory(engine)