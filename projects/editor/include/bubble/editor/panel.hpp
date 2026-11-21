#pragma once
#include <imgui.h>

namespace bubble
{
// A mouse button went down this frame over the window being drawn, or over
// one inside it: the user turned to it, and to the document it shows.
inline bool ClickedInWindow()
{
    return ImGui::IsWindowHovered( ImGuiHoveredFlags_ChildWindows ) and
           ( ImGui::IsMouseClicked( ImGuiMouseButton_Left ) or ImGui::IsMouseClicked( ImGuiMouseButton_Right ) or
             ImGui::IsMouseClicked( ImGuiMouseButton_Middle ) );
}
}
