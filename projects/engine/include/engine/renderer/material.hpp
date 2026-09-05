#pragma once
#include "engine/types/glm.hpp"
#include "engine/renderer/texture.hpp"
#include "engine/renderer/webgpu.hpp"

namespace bubble
{

// The material uniform block, laid out to match the WGSL struct exactly.
//
// The samplers that used to live inside the GLSL Material struct are not here:
// WGSL does not allow a texture or sampler inside a struct, so they became
// separate entries in the material bind group. The has*Map flags stay, and
// still drive the same branches in the shader.
struct MaterialUniforms
{
    vec4 mDiffuseColor = vec4( 1.0f );
    vec4 mSpecularColor = vec4( vec3( 0.1f ), 1.0f );
    vec4 mAmbientColor = vec4( vec3( 0.1f ), 1.0f );
    u32 mHasDiffuseMap = 0;
    u32 mHasSpecularMap = 0;
    u32 mHasNormalMap = 0;
    i32 mShininess = 32;
    f32 mShininessStrength = 1.0f;
    u32 mNormalMapping = 0;
    f32 mNormalMappingStrength = 0.1f;
    f32 mPad0 = 0.0f;
};
static_assert( sizeof( MaterialUniforms ) == 80, "MaterialUniforms must match the WGSL struct" );
// Size alone would not catch a reorder that kept the total but moved a field.
static_assert( offsetof( MaterialUniforms, mSpecularColor ) == 16, "MaterialUniforms::mSpecularColor offset must match the WGSL struct" );
static_assert( offsetof( MaterialUniforms, mAmbientColor ) == 32, "MaterialUniforms::mAmbientColor offset must match the WGSL struct" );
static_assert( offsetof( MaterialUniforms, mHasDiffuseMap ) == 48, "MaterialUniforms::mHasDiffuseMap offset must match the WGSL struct" );
static_assert( offsetof( MaterialUniforms, mShininessStrength ) == 64, "MaterialUniforms::mShininessStrength offset must match the WGSL struct" );


struct BasicMaterial
{
    BasicMaterial() = default;
    BasicMaterial( const BasicMaterial& ) = delete;
    BasicMaterial& operator=( const BasicMaterial& ) = delete;
    BasicMaterial( BasicMaterial&& ) = default;
    BasicMaterial& operator=( BasicMaterial&& ) = default;

    // Uploads the uniforms and sets the material bind group on the pass.
    void Apply( wgpu::RenderPassEncoder pass ) const;

    // Drops the cached bind group, so the next Apply rebuilds it. Call after
    // swapping a map - the bind group holds the texture views by value.
    void InvalidateBindGroup();

    Ref<Texture2D> mDiffuseMap;
    Ref<Texture2D> mSpecularMap;
    Ref<Texture2D> mNormalMap;
    vec4 mDiffuseColor = vec4( 1.0f );
    vec4 mAmbient = vec4( vec3( 0.1f ), 1.0f );
    vec4 mSpecular = vec4( vec3( 0.1f ), 1.0f );
    vec4 mEmission = vec4( 0.0f );
    i32 mShininess = 32;
    f32 mShininessStrength = 1.0f;
    f32 mNormalMapStrength = 0.1f;

private:
    void EnsureResources() const;

    // Mutable so Apply can stay const, as every call site expects. These are a
    // cache of what the material already describes, not extra state.
    mutable wgpu::raii::Buffer mUniformBuffer;
    mutable wgpu::raii::BindGroup mBindGroup;
    mutable const Texture2D* mBoundDiffuse = nullptr;
    mutable const Texture2D* mBoundSpecular = nullptr;
    mutable const Texture2D* mBoundNormal = nullptr;
};

// PBR material
// ...

}
