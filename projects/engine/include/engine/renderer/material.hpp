#pragma once
#include "engine/utils/imexp.hpp"
#include "engine/utils/types.hpp"
#include "engine/renderer/texture.hpp"
#include "engine/renderer/shader.hpp"

namespace bubble
{

struct BUBBLE_ENGINE_EXPORT BasicMaterial
{
    Ref<Texture2D> mDiffuseMap;
    Ref<Texture2D> mSpecularMap;
    Ref<Texture2D> mNormalMap;
    vec4 mDiffuseColor = vec4( 1.0f );
    vec3 mAmbientCoef = vec4( 0.1f );
    vec3 mSpecularCoef = vec4( 0.1f );
    i32 mShininess = 32;
    f32 mNormalMapStrength = 0.1f;

    BasicMaterial() = default;
    BasicMaterial( const BasicMaterial& ) = delete;
    BasicMaterial& operator=( const BasicMaterial& ) = delete;

    BasicMaterial( BasicMaterial&& ) = default;
    BasicMaterial& operator=( BasicMaterial&& ) = default;

    void Apply( const Ref<Shader>& shader ) const;
};


//struct BUBBLE_ENGINE_EXPORT ExtendedMaterial
//{
//    vector<Ref<Texture2D>> mDiffuseMaps;
//    vector<Ref<Texture2D>> mSpecularMaps;
//    vector<Ref<Texture2D>> mNormalMaps;
//    i32 mShininess = 32;
//
//    ExtendedMaterial() = default;
//    ExtendedMaterial( vector<Ref<Texture2D>>&& diffuse_maps,
//                      vector<Ref<Texture2D>>&& specular_maps,
//                      vector<Ref<Texture2D>>&& normal_maps,
//                      i32 shininess = 32 );
//
//    ExtendedMaterial( const ExtendedMaterial& ) = delete;
//    ExtendedMaterial& operator=( const ExtendedMaterial& ) = delete;
//
//    ExtendedMaterial( ExtendedMaterial&& ) = default;
//    ExtendedMaterial& operator=( ExtendedMaterial&& ) = default;
//
//    void Set( const Ref<Shader>& shader ) const;
//};


// PBR material
// ...

}