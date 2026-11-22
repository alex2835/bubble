#pragma once
#include "bubble/core/color.hpp"
#include "bubble/types/types.hpp"
#include <imgui.h>

namespace bubble
{
// The editor's colours in one place: neutral greys, and one accent - orange
// unless the preferences say otherwise, the viewport's selection outlined in
// it too - only where it means something: what is selected, turned to, held.
namespace EditorColor
{
// The accent the preferences chose, as the style holds it, and as the
// viewport's lines take it.
ImVec4 Accent();
ImU32 AccentU32();
// A button that is on - a tool, a toggle -: the accent laid thin over a
// button's grey, opaque over a picture.
ImVec4 On();
// Text.
inline constexpr ImVec4 cText{ 0.86f, 0.87f, 0.89f, 1.0f };
inline constexpr ImVec4 cTextDim{ 0.52f, 0.54f, 0.57f, 1.0f };
// What is wrong, what may be.
inline constexpr ImVec4 cError{ 0.95f, 0.43f, 0.40f, 1.0f };
inline constexpr ImVec4 cWarning{ 0.93f, 0.78f, 0.36f, 1.0f };
// A value set apart from where it comes from: an instance's override, a
// script's prop, a graph's entry.
inline constexpr ImVec4 cSet{ 0.98f, 0.83f, 0.47f, 1.0f };
// What a script printed.
inline constexpr ImVec4 cScript{ 0.58f, 0.80f, 0.96f, 1.0f };
// A prefab's instance, and a node inside one.
inline constexpr ImVec4 cPrefab{ 0.45f, 0.68f, 0.98f, 1.0f };
inline constexpr ImVec4 cPrefabInside{ 0.64f, 0.78f, 0.96f, 1.0f };
// What a game plays now.
inline constexpr ImVec4 cLive{ 0.45f, 0.86f, 0.52f, 1.0f };
}

// Dear ImGui's style as the editor has it: the text `fontSize` pixels high,
// the accent `accent`, sizes made `scale` times larger.
void ApplyEditorStyle( f32 scale, f32 fontSize, Color accent );
}
