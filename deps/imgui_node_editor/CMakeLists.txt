#GIT_REPOSITORY "https://github.com/thedmd/imgui-node-editor.git"  develop, 021aa0e (2025)
# Node graph canvas on top of Dear ImGui: pan, zoom, drag nodes, draw links,
# create and delete them by mouse. The animation controller window is a
# graph of states and transitions drawn with it. MIT, see LICENSE.
add_library(imgui_node_editor STATIC
  crude_json.cpp
  imgui_canvas.cpp
  imgui_node_editor.cpp
  imgui_node_editor_api.cpp
)
target_include_directories(imgui_node_editor PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(imgui_node_editor PUBLIC imgui)
