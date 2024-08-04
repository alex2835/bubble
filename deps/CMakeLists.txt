
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

#GIT_TAG v5.3.1  GIT_REPOSITORY "https://github.com/assimp/assimp.git"
option(ASSIMP_BUILD_TESTS "" OFF)
option(ASSIMP_BUILD_DOCS "" OFF)
option(BUILD_SHARED_LIBS "" OFF)
add_subdirectory(assimp)

#GIT_TAG 0.9.9.8  GIT_REPOSITORY "https://github.com/g-truc/glm.git" 
add_subdirectory(glm)
add_subdirectory(glm-aabb)

add_subdirectory(imgui-filebrowser)
add_subdirectory(json)
add_subdirectory(recs)
add_subdirectory(hot_reloader)
add_subdirectory(stb_image)
add_subdirectory(magic_enum)
add_subdirectory(thread_pool)
add_subdirectory(argagg)
