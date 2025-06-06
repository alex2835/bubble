cmake_minimum_required(VERSION 3.12)
 
set(TARGET_NAME bubble_editor)

file(GLOB_RECURSE SRCS *.cpp)
file(GLOB_RECURSE HDRS *.hpp)
add_executable(${TARGET_NAME} ${SRCS} ${HDRS})

target_include_directories(${TARGET_NAME} PUBLIC 
	${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_definitions(-DIMGUI_DEFINE_MATH_OPERATORS)
add_definitions(-DSOL_ALL_SAFETIES_ON)

target_link_libraries(${TARGET_NAME} bubble_engine)
target_link_libraries(${TARGET_NAME} argagg)

add_custom_command(
    TARGET ${TARGET_NAME} PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${PROJECT_SOURCE_DIR}/resources
    $<TARGET_FILE_DIR:${TARGET_NAME}>/resources)

