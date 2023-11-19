cmake_minimum_required(VERSION 3.12)
 
set(TARGET_NAME bubble)

file(GLOB_RECURSE SRCS *.cpp)
file(GLOB_RECURSE HDRS *.h)
add_executable(${TARGET_NAME} ${SRCS} ${HDRS})

target_link_libraries(${TARGET_NAME} libglew_static)
target_link_libraries(${TARGET_NAME} glfw)
target_link_libraries(${TARGET_NAME} glm::glm)
target_link_libraries(${TARGET_NAME} imgui)
target_link_libraries(${TARGET_NAME} bubble_engine)

target_include_directories(${TARGET_NAME} PUBLIC 
	${CMAKE_CURRENT_SOURCE_DIR}/include
)

# C4251 suppress stl dll error
target_compile_options(${TARGET_NAME} PRIVATE
	/wd4251 /WX
)