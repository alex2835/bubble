cmake_minimum_required(VERSION 3.12)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(wgsl_reflect STATIC
	src/wgsl_reflect.cpp
	include/wgsl_reflect/wgsl_reflect.hpp
)

target_include_directories(wgsl_reflect PUBLIC include)

# Self-contained: standard library only, no WebGPU headers. That is what makes
# it testable without a device, which matters for a layout calculator whose
# mistakes are silent - a wrong offset renders as garbage rather than an error.
option(WGSL_REFLECT_BUILD_TESTS "Build the wgsl_reflect self test" OFF)
if (WGSL_REFLECT_BUILD_TESTS)
	add_executable(wgsl_reflect_test test/test_wgsl_reflect.cpp)
	target_link_libraries(wgsl_reflect_test PRIVATE wgsl_reflect)
	enable_testing()
	add_test(NAME wgsl_reflect_test COMMAND wgsl_reflect_test)
endif()
