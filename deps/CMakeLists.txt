
# Suppres third party warnings
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	SET(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -w -stdlib=libc++")
	SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -w -stdlib=libc++")
endif()


if(NOT EMSCRIPTEN)
	#GIT_TAG glew-cmake-2.2.0  GIT_REPOSITORY "https://github.com/Perlmint/glew-cmake.git"
	add_subdirectory(glew)

	#GIT_TAG 3.3.8  GIT_REPOSITORY "https://github.com/glfw/glfw.git"
    option(GLFW_BUILD_EXAMPLES "" OFF)
	option(GLFW_BUILD_TESTS "" OFF)
	option(GLFW_BUILD_DOCS "" OFF)
	option(glew-cmake_BUILD_SHARED "" OFF)
	add_subdirectory(glfw)
endif()

#GIT_TAG v1.89.9-docking  GIT_REPOSITORY "https://github.com/ocornut/imgui.git"
add_subdirectory(imgui)
add_subdirectory(imgui_gizmo)
add_subdirectory(imgui-filebrowser)

#GIT_TAG v5.3.1  GIT_REPOSITORY "https://github.com/assimp/assimp.git"
option(ASSIMP_BUILD_TESTS "" OFF)
option(ASSIMP_BUILD_DOCS "" OFF)
option(BUILD_SHARED_LIBS "" OFF)
option(ASSIMP_NO_EXPORT "" ON)
option(ASSIMP_INJECT_DEBUG_POSTFIX "" OFF)
option(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT "" FALSE)
set(ASSIMP_BUILD_OBJ_IMPORTER TRUE)
add_subdirectory(assimp)

#GIT_TAG 0.9.9.8  GIT_REPOSITORY "https://github.com/g-truc/glm.git" 
add_subdirectory(glm)
add_subdirectory(glm-aabb)

add_subdirectory(lua)
add_subdirectory(sol2-3.5.0)
add_subdirectory(json)
add_subdirectory(recs)
#add_subdirectory(hot_reloader)
add_subdirectory(argagg)
add_subdirectory(stb_image)
add_subdirectory(magic_enum)
add_subdirectory(thread_pool)

