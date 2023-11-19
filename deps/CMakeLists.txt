include(FetchContent)

FetchContent_Declare(
  glew
  GIT_REPOSITORY "https://github.com/Perlmint/glew-cmake.git"
  GIT_TAG glew-cmake-2.2.0 )
add_subdirectory(glew)

FetchContent_Declare(
  glfw
  GIT_REPOSITORY "https://github.com/glfw/glfw.git"
  GIT_TAG 3.3.8
)
add_subdirectory(glfw)

FetchContent_Declare(
  imgui
  GIT_REPOSITORY "https://github.com/ocornut/imgui.git"
  GIT_TAG v1.89.9-docking
)
add_subdirectory(imgui)

FetchContent_Declare(
  glm
  GIT_REPOSITORY "https://github.com/g-truc/glm.git"
  GIT_TAG 0.9.9.8
)
add_subdirectory(glm)
add_subdirectory(glm-aabb)
add_subdirectory(stb_image)