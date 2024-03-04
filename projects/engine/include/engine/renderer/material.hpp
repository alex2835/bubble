#pragma once
#include "engine/utils/imexp.hpp"
#include "engine/utils/types.hpp"
#include "engine/renderer/texture.hpp"
#include "engine/renderer/shader.hpp"

namespace bubble
{

struct BUBBLE_ENGINE_EXPORT BasicMaterial
{
    BasicMaterial() = default;
    BasicMaterial( const BasicMaterial& ) = delete;
    BasicMaterial& operator=( const BasicMaterial& ) = delete;
    BasicMaterial( BasicMaterial&& ) = default;
    BasicMaterial& operator=( BasicMaterial&& ) = default;

    void Apply( const Ref<Shader>& shader ) const;

    Ref<Texture2D> mDiffuseMap;
    Ref<Texture2D> mSpecularMap;
    Ref<Texture2D> mNormalMap;
    vec4 mDiffuseColor = vec4( 1.0f );
    vec3 mAmbientCoef = vec4( 0.1f );
    vec3 mSpecularCoef = vec4( 0.1f );
    i32 mShininess = 32;
    f32 mNormalMapStrength = 0.1f;
};

// PBR material
// ...

}