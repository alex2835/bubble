#pragma once
#include "bubble/types/containers.hpp"
#include "bubble/types/number.hpp"
#include "bubble/types/opt_ref.hpp"
#include "bubble/types/string.hpp"

namespace bubble
{
enum class MaterialParamKind : u8
{
    Number,
    Texture,
};

// What a texture parameter is until one is given: white, black, or a flat
// normal.
enum class TextureDefault : u8
{
    White,
    Black,
    Normal,
};

// One parameter of a material's shader, as its WGSL declares it.
struct MaterialParam
{
    string mName;
    MaterialParamKind mKind = MaterialParamKind::Number;
    // A number: where it is in the material's uniform block, how many
    // floats - 1 for an f32, 2 to 4 for a vector - whether it is a color
    // (given in sRGB, as picked, and turned linear for the shader), and what
    // it is when no material sets it.
    u32 mOffset = 0;
    u32 mComponents = 1;
    bool mColor = false;
    vector<f32> mDefault;
    // A texture: its binding in group 2.
    u32 mBinding = 0;
    TextureDefault mTextureDefault = TextureDefault::White;
};

// What a material's WGSL declares, as the engine takes it. The contract:
//
//     struct Material
//     {
//         base_color: vec4f, // @color @default( 1, 1, 1, 1 )
//         roughness: f32,    // @default( 0.5 )
//     }
//     @group( 2 ) @binding( 0 ) var<uniform> material: Material;
//     @group( 2 ) @binding( 1 ) var material_sampler: sampler;
//     @group( 2 ) @binding( 2 ) var base_color_map: texture_2d<f32>; // @default( white )
//
//     fn surface( in: SurfaceInput ) -> SurfaceOutput { ... }
//
// Everything it binds is in group 2: at most one uniform struct, whose
// fields - f32 and vec2f to vec4f - are its number parameters; 2D float
// textures, its texture parameters; samplers, which the engine gives. The
// comment on a field's or a texture's line may give its default and say a
// vector is a color. SurfaceInput, SurfaceOutput and the helpers come from
// the engine (projects/engine/shaders/lit.wgsl).
struct MaterialLayout
{
    // The uniform block's binding and size; none when it declares no struct.
    opt<u32> mUniformBinding;
    u32 mUniformSize = 0;
    vector<MaterialParam> mParams;
    vector<u32> mSamplers;

    OptRef<const MaterialParam> TryFind( string_view name ) const;
};

// The error starts with `label` - the shader's path - and says what breaks
// the contract.
expected<MaterialLayout, string> TryReflectMaterial( string_view source, string_view label );
}
