cmake_minimum_required(VERSION 3.12)
 
set(TARGET_NAME imgui_gizmo)
set(CMAKE_CXX_STANDARD 23)

file(GLOB_RECURSE SRCS *.cpp)
file(GLOB_RECURSE HDRS *.h)
add_library(${TARGET_NAME} STATIC ${SRCS} ${HDRS})

target_link_libraries(${TARGET_NAME} imgui)

target_include_directories(${TARGET_NAME} PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}
)
