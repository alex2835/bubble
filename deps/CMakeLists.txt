include(FetchContent)


if(NOT EMSCRIPTEN)
#FetchContent_Declare(
#  glew
#  GIT_REPOSITORY "https://github.com/Perlmint/glew-cmake.git"
#  GIT_TAG glew-cmake-2.2.0 )
add_subdirectory(glew)

#FetchContent_Declare(
#  glfw
#  GIT_REPOSITORY "https://github.com/glfw/glfw.git"
#  GIT_TAG 3.3.8
#)
option(GLFW_BUILD_TESTS "" OFF)
option(GLFW_BUILD_DOCS "" OFF)
add_subdirectory(glfw)
endif()

#FetchContent_Declare(
#  imgui
#  GIT_REPOSITORY "https://github.com/ocornut/imgui.git"
#  GIT_TAG v1.89.9-docking
#)
add_subdirectory(imgui)

#FetchContent_Declare(
#  assimp
#  GIT_REPOSITORY "https://github.com/assimp/assimp.git"
#  GIT_TAG v5.3.1
#)
option(ASSIMP_BUILD_TESTS "" OFF)
option(ASSIMP_BUILD_DOCS "" OFF)
option(BUILD_SHARED_LIBS "" OFF)
add_subdirectory(assimp)

#FetchContent_Declare(
#  glm
#  GIT_REPOSITORY "https://github.com/g-truc/glm.git"
#  GIT_TAG 0.9.9.8
#)
add_subdirectory(glm)
add_subdirectory(glm-aabb)
add_subdirectory(stb_image)