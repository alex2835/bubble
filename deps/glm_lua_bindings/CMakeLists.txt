cmake_minimum_required(VERSION 3.12)
project(glm_lua_bindings)
set(CMAKE_CXX_STANDARD 23)

file(GLOB_RECURSE SRCS *.cpp)
file(GLOB_RECURSE HDRS *.hpp)
add_library(glm_lua_bindings STATIC ${SRCS} ${HDRS})

target_link_libraries(glm_lua_bindings PUBLIC glm)
target_link_libraries(glm_lua_bindings PUBLIC lua)
target_link_libraries(glm_lua_bindings PUBLIC sol2)

target_include_directories(glm_lua_bindings PUBLIC 
	${CMAKE_CURRENT_SOURCE_DIR}/include
)
