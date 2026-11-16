#pragma once
#include "bubble/assets/asset_id.hpp"
#include "bubble/types/containers.hpp"
#include "bubble/types/number.hpp"
#include "bubble/types/string.hpp"
#include "bubble/types/utility.hpp"

namespace bubble
{
class AssetRegistry;

// What a material gives one parameter of its shader: numbers - one for an
// f32, two to four for a vector, a color as picked, in sRGB - or a texture.
struct MaterialValue
{
    vector<f32> mNumbers;
    // Set for a texture, and then the numbers are empty.
    AssetId mTexture;

    friend bool operator==( const MaterialValue&, const MaterialValue& ) = default;
};

// A material as its file has it: a shader and values for its parameters,
// or a base material and only what differs from it - the rest, the shader
// included, comes from the base, as a prefab's instance takes what it does
// not override. A parameter set nowhere has the shader's default.
struct MaterialAsset
{
    // The WGSL with its surface(); none is the engine's standard material.
    // A material with a base takes its base's.
    AssetId mShader;
    AssetId mBase;
    // Drawn from behind too: no faces culled, the back's normal turned.
    opt<bool> mDoubleSided;
    // Drawn after everything opaque, blended by its alpha.
    opt<bool> mTransparent;
    // By the parameter's name, as the shader declares it.
    vector<pair<string, MaterialValue>> mParams;

    friend bool operator==( const MaterialAsset&, const MaterialAsset& ) = default;
};

constexpr string_view cMaterialKind = "material";

// A .mat file and the imported material alike are JSON:
//
//     { "version": 1,
//       "base": { "asset": "5d2a...", "path": "materials/metal.mat" },
//       "double_sided": true,
//       "params": { "base_color": [ 0.8, 0.1, 0.1, 1 ], "roughness": 0.3,
//                   "base_color_map": { "asset": "9c41...", "path": "images/rust.png" } } }
//
// An asset is named by its id; the path is for people. The error says
// where and what is wrong.
expected<MaterialAsset, string> TryReadMaterialAsset( string_view text );
string WriteMaterialAsset( const MaterialAsset& material );

// Teaches the registry materials: <id>.material.
void RegisterMaterialLoader( AssetRegistry& registry );
}
