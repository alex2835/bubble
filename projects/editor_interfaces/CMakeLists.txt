cmake_minimum_required(VERSION 3.12)
 
set(TARGET_NAME bubble_editor_interface)

file(GLOB_RECURSE SRCS *.cpp)
file(GLOB_RECURSE HDRS *.h)
add_library(${TARGET_NAME} SHARED ${SRCS} ${HDRS})

add_definitions(-DIMGUI_DEFINE_MATH_OPERATORS)

target_link_libraries(${TARGET_NAME} bubble_engine)
target_link_libraries(${TARGET_NAME} hot_reloader_export)

target_include_directories(${TARGET_NAME} PUBLIC 
	${CMAKE_CURRENT_SOURCE_DIR}/include
	${CMAKE_CURRENT_SOURCE_DIR}/../editor/include
)

