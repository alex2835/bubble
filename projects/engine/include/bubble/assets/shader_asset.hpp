#pragma once
#include "bubble/types/string.hpp"

namespace bubble
{
class AssetRegistry;

// A material's own WGSL: its parameters, its textures and its surface(),
// which the engine builds into a lit shader (bubble/render/material_layout.hpp
// says what it may declare).
struct ShaderAsset
{
    string mSource;
};

constexpr string_view cShaderKind = "shader";

// Teaches the registry shaders: <id>.shader, the WGSL as it is.
void RegisterShaderLoader( AssetRegistry& registry );
}
