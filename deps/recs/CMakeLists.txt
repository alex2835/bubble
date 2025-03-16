cmake_minimum_required(VERSION 3.18)
project(recs VERSION 0.1.0)
set(CMAKE_CXX_STANDARD 23)

SET(SOURCE
   "src/pool.cpp"
   "src/registry.cpp"
)

add_library(recs STATIC ${SOURCE})
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
	target_compile_options(recs PUBLIC
		/WX
	)
endif()
add_definitions(-DBUILDING_RECS)

target_include_directories(recs PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include
)

option(RECS_TEST "" OFF)
if( RECS_TEST )
	add_definitions(-DRECS_STATIC)
	add_executable(recs_test tests/test.cpp)
	target_link_libraries(recs_test recs)
endif()
