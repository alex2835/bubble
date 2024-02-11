
if(NOT EMSCRIPTEN)
	#GIT_REPOSITORY "https://github.com/Perlmint/glew-cmake.git"
	#GIT_TAG glew-cmake-2.2.0 )
	add_subdirectory(glew)

	#GIT_REPOSITORY "https://github.com/glfw/glfw.git"
	#GIT_TAG 3.3.8
    option(GLFW_BUILD_EXAMPLES "" OFF)
	option(GLFW_BUILD_TESTS "" OFF)
	option(GLFW_BUILD_DOCS "" OFF)
	add_subdirectory(glfw)
endif()

#GIT_REPOSITORY "https://github.com/ocornut/imgui.git"
#GIT_TAG v1.89.9-docking
add_subdirectory(imgui)

#GIT_REPOSITORY "https://github.com/assimp/assimp.git"
#GIT_TAG v5.3.1
option(ASSIMP_BUILD_TESTS "" OFF)
option(ASSIMP_BUILD_DOCS "" OFF)
option(BUILD_SHARED_LIBS "" OFF)
add_subdirectory(assimp)

#GIT_REPOSITORY "https://github.com/g-truc/glm.git"
#GIT_TAG 0.9.9.8
add_subdirectory(glm)

add_subdirectory(imgui_gizmo)
add_subdirectory(glm-aabb)
add_subdirectory(stb_image)
add_subdirectory(recs)
add_subdirectory(hot_reloader)
