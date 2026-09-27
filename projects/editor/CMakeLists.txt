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

# A windowed application: no terminal window opens beside the editor. What it
# logs is in its Console window and in bubble_editor.log next to it. For a
# terminal while debugging, configure with -DBUBBLE_EDITOR_CONSOLE=ON.
option(BUBBLE_EDITOR_CONSOLE "Open a terminal window with the editor" OFF)
if(WIN32 AND NOT BUBBLE_EDITOR_CONSOLE)
    set_target_properties(${TARGET_NAME} PROPERTIES WIN32_EXECUTABLE TRUE)
    if(MSVC)
        # main(), not WinMain.
        target_link_options(${TARGET_NAME} PRIVATE /ENTRY:mainCRTStartup)
    endif()
endif()

target_link_libraries(${TARGET_NAME} bubble_engine)
target_link_libraries(${TARGET_NAME} argagg)
target_link_libraries(${TARGET_NAME} imgui_node_editor)

# Copies wgpu_native.dll next to the executable. No-op for the Emscripten
# backends, which link the implementation in.
target_copy_webgpu_binaries(${TARGET_NAME})

add_custom_command(
    TARGET ${TARGET_NAME} PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${PROJECT_SOURCE_DIR}/resources
    $<TARGET_FILE_DIR:${TARGET_NAME}>/resources)

