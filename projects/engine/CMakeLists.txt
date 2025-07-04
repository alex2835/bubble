cmake_minimum_required(VERSION 3.12)
 
set(TARGET_NAME bubble_engine)

file(GLOB_RECURSE SRCS *.cpp)
file(GLOB_RECURSE HDRS *.hpp)
add_library(${TARGET_NAME} STATIC ${SRCS} ${HDRS})

target_include_directories(${TARGET_NAME} PUBLIC 
	${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_definitions(-DGLM_ENABLE_EXPERIMENTAL)
add_definitions(-DGLM_FORCE_SWIZZLE)
add_definitions(-DSOL_ALL_SAFETIES_ON)

target_link_libraries(${TARGET_NAME} PUBLIC imgui)
target_link_libraries(${TARGET_NAME} PUBLIC imgui_gizmo)
target_link_libraries(${TARGET_NAME} PUBLIC imguifilebrowser)
target_link_libraries(${TARGET_NAME} PUBLIC glm)
target_link_libraries(${TARGET_NAME} PUBLIC glm_aabb)
target_link_libraries(${TARGET_NAME} PUBLIC stb_image)
target_link_libraries(${TARGET_NAME} PUBLIC recs)
target_link_libraries(${TARGET_NAME} PUBLIC magic_enum)
target_link_libraries(${TARGET_NAME} PUBLIC thread_pool)
target_link_libraries(${TARGET_NAME} PUBLIC assimp)
target_link_libraries(${TARGET_NAME} PUBLIC nlohmann_json)
target_link_libraries(${TARGET_NAME} PUBLIC lua)
target_link_libraries(${TARGET_NAME} PUBLIC sol2)
target_link_libraries(${TARGET_NAME} PUBLIC glm_lua_bindings)
target_link_libraries(${TARGET_NAME} PUBLIC BulletIncludeFile)
target_link_libraries(${TARGET_NAME} PUBLIC LinearMath)
target_link_libraries(${TARGET_NAME} PUBLIC BulletCollision)
target_link_libraries(${TARGET_NAME} PUBLIC BulletDynamics)

target_precompile_headers(${TARGET_NAME} PUBLIC "include/engine/pch/pch.hpp")

if(EMSCRIPTEN)
	target_compile_definitions(${TARGET_NAME} PUBLIC -DBUBBLE_STATIC)
	target_link_options(${TARGET_NAME} PUBLIC
		--bind -sUSE_WEBGL2=1 -sUSE_GLFW=3 -sWASM=1
	)
else()
	target_link_libraries(${TARGET_NAME} PUBLIC libglew_static)
	target_link_libraries(${TARGET_NAME} PUBLIC glfw)
endif()

#set_target_properties(${TARGET_NAME} PROPERTIES UNITY_BUILD ON)
#set_target_properties(${TARGET_NAME} PROPERTIES UNITY_BUILD_BATCH_SIZE 4)
