cmake_minimum_required(VERSION 3.12)
 
set(TARGET_NAME bubble_engine)

file(GLOB_RECURSE SRCS *.cpp)
file(GLOB_RECURSE HDRS *.h)
add_library(${TARGET_NAME} SHARED ${SRCS} ${HDRS})
add_definitions(-DBUILDING_BUBBLE_ENGINE)

target_link_libraries(${TARGET_NAME} libglew_static)
target_link_libraries(${TARGET_NAME} glfw)
target_link_libraries(${TARGET_NAME} glm::glm)
target_link_libraries(${TARGET_NAME} imgui)

target_include_directories(${TARGET_NAME} PUBLIC 
	${CMAKE_CURRENT_SOURCE_DIR}/include
)
