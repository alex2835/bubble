#include "bubble/editor/editor_style.hpp"

namespace bubble
{
namespace
{
constexpr ImVec4 Grey( f32 value, f32 alpha = 1.0f )
{
    // A touch of blue keeps the greys from looking brown beside a warm accent.
    return ImVec4( value, value + 0.004f, value + 0.014f, alpha );
}

// The style's colour the accent is kept in.
constexpr ImGuiCol cAccentSlot = ImGuiCol_CheckMark;
}

ImVec4 EditorColor::Accent()
{
    return ImGui::GetStyle().Colors[cAccentSlot];
}

ImU32 EditorColor::AccentU32()
{
    return ImGui::GetColorU32( Accent() );
}

ImVec4 EditorColor::On()
{
    const ImVec4 accent = Accent();
    const ImVec4 grey = Grey( 0.19f );
    const auto mix = []( f32 over, f32 under ) { return over * 0.4f + under * 0.6f; };
    return ImVec4( mix( accent.x, grey.x ), mix( accent.y, grey.y ), mix( accent.z, grey.z ), 1.0f );
}

void ApplyEditorStyle( f32 scale, f32 fontSize, Color accentColor )
{
    const ImVec4 accent( accentColor.mR, accentColor.mG, accentColor.mB, 1.0f );
    const auto faded = [&]( f32 alpha ) { return ImVec4( accent.x, accent.y, accent.z, alpha ); };
    ImGuiStyle& style = ImGui::GetStyle();
    style = ImGuiStyle();
    style.FontSizeBase = fontSize;
    // A little air and a little roundness; borders thin, the rest flat.
    style.WindowPadding = ImVec2( 8, 8 );
    style.FramePadding = ImVec2( 6, 4 );
    style.ItemSpacing = ImVec2( 8, 5 );
    style.ItemInnerSpacing = ImVec2( 5, 4 );
    style.IndentSpacing = 16;
    style.ScrollbarSize = 12;
    style.GrabMinSize = 10;
    style.WindowRounding = 4;
    style.ChildRounding = 3;
    style.FrameRounding = 3;
    style.PopupRounding = 4;
    style.ScrollbarRounding = 6;
    style.GrabRounding = 3;
    style.TabRounding = 3;
    style.WindowBorderSize = 1;
    style.FrameBorderSize = 0;
    style.PopupBorderSize = 1;
    style.TabBorderSize = 0;
    style.TabBarOverlineSize = 2;
    style.DockingSeparatorSize = 2;
    style.WindowMenuButtonPosition = ImGuiDir_None;
    style.SeparatorTextBorderSize = 1;

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text] = EditorColor::cText;
    colors[ImGuiCol_TextDisabled] = EditorColor::cTextDim;
    colors[ImGuiCol_WindowBg] = Grey( 0.118f );
    colors[ImGuiCol_ChildBg] = ImVec4( 0, 0, 0, 0 );
    colors[ImGuiCol_PopupBg] = Grey( 0.137f, 0.98f );
    colors[ImGuiCol_Border] = Grey( 0.20f );
    colors[ImGuiCol_BorderShadow] = ImVec4( 0, 0, 0, 0 );
    colors[ImGuiCol_FrameBg] = Grey( 0.17f );
    colors[ImGuiCol_FrameBgHovered] = Grey( 0.21f );
    colors[ImGuiCol_FrameBgActive] = Grey( 0.24f );
    colors[ImGuiCol_TitleBg] = Grey( 0.09f );
    colors[ImGuiCol_TitleBgActive] = Grey( 0.10f );
    colors[ImGuiCol_TitleBgCollapsed] = Grey( 0.09f );
    colors[ImGuiCol_MenuBarBg] = Grey( 0.10f );
    colors[ImGuiCol_ScrollbarBg] = ImVec4( 0, 0, 0, 0 );
    colors[ImGuiCol_ScrollbarGrab] = Grey( 0.25f );
    colors[ImGuiCol_ScrollbarGrabHovered] = Grey( 0.31f );
    colors[ImGuiCol_ScrollbarGrabActive] = Grey( 0.37f );
    colors[cAccentSlot] = accent;
    colors[ImGuiCol_SliderGrab] = Grey( 0.42f );
    colors[ImGuiCol_SliderGrabActive] = accent;
    colors[ImGuiCol_Button] = Grey( 0.19f );
    colors[ImGuiCol_ButtonHovered] = Grey( 0.24f );
    colors[ImGuiCol_ButtonActive] = Grey( 0.29f );
    // Rows: hovered grey, selected in the accent.
    colors[ImGuiCol_Header] = faded( 0.26f );
    colors[ImGuiCol_HeaderHovered] = Grey( 0.22f );
    colors[ImGuiCol_HeaderActive] = faded( 0.42f );
    colors[ImGuiCol_Separator] = Grey( 0.20f );
    colors[ImGuiCol_SeparatorHovered] = faded( 0.60f );
    colors[ImGuiCol_SeparatorActive] = accent;
    colors[ImGuiCol_ResizeGrip] = ImVec4( 0, 0, 0, 0 );
    colors[ImGuiCol_ResizeGripHovered] = faded( 0.60f );
    colors[ImGuiCol_ResizeGripActive] = accent;
    colors[ImGuiCol_InputTextCursor] = EditorColor::cText;
    colors[ImGuiCol_Tab] = Grey( 0.10f );
    colors[ImGuiCol_TabHovered] = Grey( 0.20f );
    colors[ImGuiCol_TabSelected] = Grey( 0.118f );
    colors[ImGuiCol_TabSelectedOverline] = accent;
    colors[ImGuiCol_TabDimmed] = Grey( 0.10f );
    colors[ImGuiCol_TabDimmedSelected] = Grey( 0.118f );
    colors[ImGuiCol_TabDimmedSelectedOverline] = Grey( 0.30f );
    colors[ImGuiCol_DockingPreview] = faded( 0.45f );
    colors[ImGuiCol_DockingEmptyBg] = Grey( 0.08f );
    colors[ImGuiCol_PlotLines] = Grey( 0.60f );
    colors[ImGuiCol_PlotLinesHovered] = accent;
    colors[ImGuiCol_PlotHistogram] = Grey( 0.60f );
    colors[ImGuiCol_PlotHistogramHovered] = accent;
    colors[ImGuiCol_TableHeaderBg] = Grey( 0.15f );
    colors[ImGuiCol_TableBorderStrong] = Grey( 0.20f );
    colors[ImGuiCol_TableBorderLight] = Grey( 0.16f );
    colors[ImGuiCol_TableRowBg] = ImVec4( 0, 0, 0, 0 );
    colors[ImGuiCol_TableRowBgAlt] = ImVec4( 1, 1, 1, 0.025f );
    colors[ImGuiCol_TextLink] = accent;
    colors[ImGuiCol_TextSelectedBg] = faded( 0.35f );
    colors[ImGuiCol_TreeLines] = Grey( 0.25f );
    colors[ImGuiCol_DragDropTarget] = accent;
    colors[ImGuiCol_NavCursor] = accent;
    colors[ImGuiCol_NavWindowingHighlight] = ImVec4( 1, 1, 1, 0.7f );
    colors[ImGuiCol_NavWindowingDimBg] = ImVec4( 0, 0, 0, 0.5f );
    colors[ImGuiCol_ModalWindowDimBg] = ImVec4( 0, 0, 0, 0.55f );

    style.ScaleAllSizes( scale );
    style.FontScaleDpi = scale;
}
}
