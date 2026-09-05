
set(CMAKE_CXX_STANDARD 23)
set(imgui_SOURCE_DIR "imgui-1.92.0-docking")

add_library(imgui STATIC
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui.h
  ${imgui_SOURCE_DIR}/imconfig.h
  ${imgui_SOURCE_DIR}/imgui_demo.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_internal.h
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/imstb_rectpack.h
  ${imgui_SOURCE_DIR}/imstb_textedit.h
  ${imgui_SOURCE_DIR}/imstb_truetype.h
  ${imgui_SOURCE_DIR}/backends/imgui_impl_wgpu.h
  ${imgui_SOURCE_DIR}/backends/imgui_impl_wgpu.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.h
  ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
)

target_include_directories(imgui PUBLIC
	${imgui_SOURCE_DIR}
	${imgui_SOURCE_DIR}/backends
)
target_link_libraries(imgui PUBLIC glfw webgpu)

# ImGui's WebGPU backend has to be told which implementation it is compiling
# against - the two differ enough that it refuses to guess. Derived from the
# distribution's WEBGPU_BACKEND so switching backends stays one -D.
string(TOUPPER "${WEBGPU_BACKEND}" _IMGUI_WEBGPU_BACKEND_U)
if (_IMGUI_WEBGPU_BACKEND_U STREQUAL "WGPU")
	target_compile_definitions(imgui PUBLIC IMGUI_IMPL_WEBGPU_BACKEND_WGPU)
elseif (_IMGUI_WEBGPU_BACKEND_U STREQUAL "DAWN" OR _IMGUI_WEBGPU_BACKEND_U STREQUAL "EMDAWNWEBGPU")
	# emdawnwebgpu is Dawn compiled to wasm, so it takes the Dawn path.
	target_compile_definitions(imgui PUBLIC IMGUI_IMPL_WEBGPU_BACKEND_DAWN)
else()
	message(FATAL_ERROR "imgui: unhandled WEBGPU_BACKEND '${WEBGPU_BACKEND}'")
endif()
