cmake_minimum_required(VERSION 3.12)
 
set(TARGET_NAME bubble)

file(GLOB_RECURSE SRCS *.cpp)
file(GLOB_RECURSE HDRS *.h)
add_executable(${TARGET_NAME} ${SRCS} ${HDRS})

target_link_libraries(${TARGET_NAME} bubble_engine)
target_link_libraries(${TARGET_NAME} hot_reloader_import)

target_include_directories(${TARGET_NAME} PUBLIC 
	${CMAKE_CURRENT_SOURCE_DIR}/include
	${CMAKE_CURRENT_SOURCE_DIR}/../editor_interface/include
)
