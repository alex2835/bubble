#pragma once
#include "bubble/core/color.hpp"
#include "bubble/types/types.hpp"
#include <imgui.h>

namespace bubble
{
// The editor's colours in one place: neutral greys, and one accent - a
// muted blue unless the preferences say otherwise, the viewport's selection outlined in
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
// The project's files by kind: the stripe under a tile in the assets panel,
// as Unreal's content browser marks its assets - a level orange, a material
// green; a prefab the blue the tree shows one in.
inline constexpr ImVec4 cKindLevel{ 0.96f, 0.58f, 0.18f, 1.0f };
inline constexpr ImVec4 cKindPrefab = cPrefab;
inline constexpr ImVec4 cKindModel{ 0.25f, 0.80f, 0.80f, 1.0f };
inline constexpr ImVec4 cKindMaterial{ 0.40f, 0.78f, 0.35f, 1.0f };
inline constexpr ImVec4 cKindTexture{ 0.88f, 0.33f, 0.30f, 1.0f };
inline constexpr ImVec4 cKindSound{ 0.62f, 0.45f, 0.90f, 1.0f };
inline constexpr ImVec4 cKindGraph{ 0.30f, 0.45f, 0.95f, 1.0f };
inline constexpr ImVec4 cKindScript{ 0.93f, 0.82f, 0.30f, 1.0f };
inline constexpr ImVec4 cKindShader{ 0.65f, 0.90f, 0.55f, 1.0f };
inline constexpr ImVec4 cKindFont{ 0.62f, 0.64f, 0.67f, 1.0f };
}

// Dear ImGui's style as the editor has it: the text `fontSize` pixels high,
// the accent `accent`, sizes made `scale` times larger.
void ApplyEditorStyle( f32 scale, f32 fontSize, Color accent );
}
