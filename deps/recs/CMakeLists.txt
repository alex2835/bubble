cmake_minimum_required(VERSION 3.18)
project(recs VERSION 0.1.0)
set(CMAKE_CXX_STANDARD 23)
enable_testing()

SET(SOURCE
   "src/pool.cpp"
   "src/registry.cpp"
)

add_library(recs STATIC ${SOURCE})

# PRIVATE: warnings-as-errors is our own build policy. As PUBLIC it was applied
# to every consumer of recs, i.e. the whole engine and editor.
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
	target_compile_options(recs PRIVATE
		/WX
	)
endif()

# The headers use std::format, concepts and ranges, so consumers need C++23 too.
# CMAKE_CXX_STANDARD alone does not propagate through target_link_libraries.
target_compile_features(recs PUBLIC cxx_std_23)

target_include_directories(recs PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include
)

option(RECS_TEST "Build the recs test suite" OFF)
if( RECS_TEST )
	add_executable(recs_test tests/test.cpp)
	target_link_libraries(recs_test recs)
	add_test(NAME recs_test COMMAND recs_test)
endif()

option(RECS_BENCHMARK "Build the recs microbenchmarks" OFF)
if( RECS_BENCHMARK )
	add_executable(recs_benchmark benchmarks/benchmark.cpp)
	target_link_libraries(recs_benchmark recs)
endif()
