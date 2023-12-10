message(STATUS "Fetching imgui ...")

#FetchContent_GetProperties(imgui)
#if (NOT imgui_POPULATED)
#  FetchContent_Populate(imgui)
#endif ()

set(imgui_SOURCE_DIR "imgui")

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
  ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.h
  ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.h
  ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
)
target_include_directories(imgui PUBLIC 
	${imgui_SOURCE_DIR}
	${imgui_SOURCE_DIR}/backends
)
target_link_libraries(imgui PUBLIC glfw)

#if(EMSCRIPTEN)
#set_target_properties(imgui
#        PROPERTIES SUFFIX ".html"
#        LINK_FLAGS " --bind -s USE_WEBGL2=1 -s USE_GLFW=3 -s WASM=1"
#)
#endif()

#FetchContent_MakeAvailable(imgui)