#include "engine/pch/pch.hpp"
#include "engine/renderer/material.hpp"
#include "engine/renderer/pipeline.hpp"
#include "engine/renderer/gpu_context.hpp"

namespace bubble
{

void BasicMaterial::InvalidateBindGroup()
{
    mBindGroup = {};
    mBoundDiffuse = nullptr;
    mBoundSpecular = nullptr;
    mBoundNormal = nullptr;
}

void BasicMaterial::EnsureResources() const
{
    if ( not mUniformBuffer )
    {
        wgpu::BufferDescriptor desc = wgpu::Default;
        desc.label = wgpu::StringView( "Material Uniforms" );
        desc.size = sizeof( MaterialUniforms );
        desc.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
        desc.mappedAtCreation = false;
        mUniformBuffer = wgpu::raii::Buffer( Gpu().Device().createBuffer( desc ) );
    }

    // A material with no map of a given kind still has to fill that binding -
    // a bind group must match its layout exactly. Binding a 1x1 white texture
    // keeps one layout and one pipeline serving every material, which is much
    // cheaper than a pipeline variant per combination of present maps.
    const Texture2D& white = Gpu().WhiteTexture();
    const Texture2D* diffuse  = mDiffuseMap  ? mDiffuseMap.get()  : &white;
    const Texture2D* specular = mSpecularMap ? mSpecularMap.get() : &white;
    const Texture2D* normal   = mNormalMap   ? mNormalMap.get()   : &white;

    if ( mBindGroup and
         diffuse == mBoundDiffuse and
         specular == mBoundSpecular and
         normal == mBoundNormal )
        return;

    array<wgpu::BindGroupEntry, 5> entries = {};
    entries[0] = {};
    entries[0].binding = 0;
    entries[0].buffer = *mUniformBuffer;
    entries[0].offset = 0;
    entries[0].size = sizeof( MaterialUniforms );

    const Texture2D* maps[3] = { diffuse, specular, normal };
    for ( u32 i = 0; i < 3; i++ )
    {
        entries[i + 1] = {};
        entries[i + 1].binding = i + 1;
        entries[i + 1].textureView = maps[i]->View();
    }

    entries[4] = {};
    entries[4].binding = 4;
    entries[4].sampler = diffuse->Sampler();

    wgpu::BindGroupDescriptor desc = wgpu::Default;
    desc.label = wgpu::StringView( "Material Bind Group" );
    desc.layout = Gpu().Layouts().Material();
    desc.entryCount = entries.size();
    desc.entries = entries.data();
    mBindGroup = wgpu::raii::BindGroup( Gpu().Device().createBindGroup( desc ) );

    mBoundDiffuse = diffuse;
    mBoundSpecular = specular;
    mBoundNormal = normal;
}

void BasicMaterial::Apply( wgpu::RenderPassEncoder pass ) const
{
    EnsureResources();

    // Written every time rather than tracked with a dirty flag: it is eighty
    // bytes, and the inspector can change any of these fields between frames.
    MaterialUniforms uniforms;
    uniforms.mDiffuseColor = mDiffuseColor;
    uniforms.mSpecularColor = mSpecular;
    uniforms.mAmbientColor = mAmbient;
    uniforms.mHasDiffuseMap = mDiffuseMap ? 1u : 0u;
    uniforms.mHasSpecularMap = mSpecularMap ? 1u : 0u;
    uniforms.mHasNormalMap = mNormalMap ? 1u : 0u;
    uniforms.mShininess = mShininess;
    uniforms.mShininessStrength = mShininessStrength;
    uniforms.mNormalMapping = mNormalMap ? 1u : 0u;
    uniforms.mNormalMappingStrength = mNormalMapStrength;

    Gpu().Queue().writeBuffer( *mUniformBuffer, 0, &uniforms, sizeof( uniforms ) );
    pass.setBindGroup( (u32)BindGroupIndex::Material, *mBindGroup, 0, nullptr );
}

}
