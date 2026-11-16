// What the renderer's tests share: a project's import cache in memory, a
// registry over it that reads models, materials, shaders and textures, App
// with the transform and render modules, a GPU, the renderer, and a square
// RGBA8 target whose pixels come back. Not on the web, as gpu_test.cpp.
#pragma once
#ifndef __EMSCRIPTEN__
#include "bubble/assets/model_primitives.hpp"
#include "bubble/assets/shader_asset.hpp"
#include "bubble/render/gpu/renderer.hpp"
#include "bubble/scene/hierarchy.hpp"
#include "bubble/world/transform.hpp"
#include "bubble/world/world.hpp"
#include "render/gpu_helpers.hpp"

namespace bubble::test
{
inline Text Make( string_view utf8 )
{
    return *Text::TryFrom( utf8 );
}

struct RenderFixture
{
    // Before App: Jobs finishes the loads still waiting when it goes, and
    // they read it.
    hmap<string, string> mImported;
    AssetCatalog mCatalog;
    App mApp{ AppOptions{ .mThreads = 1 } };
    AssetRegistry mAssets{ mCatalog, AssetRegistry::AtOnce( [this]( const AssetId& assetId, string_view kind ) {
                               const auto found = mImported.find( ImportedPath( assetId, kind ).View() );
                               if ( found == mImported.end() )
                                   return expected<string, string>( unexpected( "no such file"s ) );
                               return expected<string, string>( found->second );
                           } ),
                           mApp.GetJobs() };
    Scope<Gpu> mGpu = StartGpu();
    Renderer mRenderer{ *mGpu, mAssets };
    GpuTexture mTarget;

    explicit RenderFixture( i32 size = 64 )
        : mTarget( *mGpu,
                   GpuTextureOptions{ .mSize = ivec2( size ),
                                      .mMips = 1,
                                      .mFormat = WGPUTextureFormat_RGBA8UnormSrgb,
                                      .mUsage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopySrc,
                                      .mLabel = "target" } )
    {
        RegisterModelLoader( mAssets );
        RegisterMaterialLoader( mAssets );
        RegisterShaderLoader( mAssets );
        RegisterTextureLoader( mAssets );
        mApp.AddModule( TransformModule() );
        mApp.AddModule( RenderModule( mAssets ) );
    }

    // The importer's output for the asset at `path` - again, if it was.
    AssetId Import( string_view path, string bytes, string_view kind )
    {
        const AssetPath assetPath = *AssetPath::TryFrom( path );
        auto assetIdMaybe = mCatalog.TryIdOf( assetPath );
        if ( not assetIdMaybe )
        {
            assetIdMaybe = AssetId::Generate();
            mCatalog.Add( *assetIdMaybe, assetPath );
        }
        mImported[ImportedPath( *assetIdMaybe, kind ).String()] = std::move( bytes );
        return *assetIdMaybe;
    }

    AssetId ImportModel( string_view path, const ModelAsset& model )
    {
        return Import( path, WriteModelAsset( model ), cModelKind );
    }

    AssetId ImportMaterial( string_view path, const string& json )
    {
        const auto materialMaybe = TryReadMaterialAsset( json );
        REQUIRE_MESSAGE( materialMaybe, materialMaybe.error() );
        return Import( path, WriteMaterialAsset( *materialMaybe ), cMaterialKind );
    }

    // `frames` frames of the world - enough for loads, the GPU's checks and
    // textures to come - each drawn through its first camera; the last
    // one's pixels.
    vector<u8> Draw( World& world, u32 frames = 8 )
    {
        for ( u32 frame = 0; frame < frames; ++frame )
        {
            world.Frame( 1.0 / 60.0 );
            mApp.GetJobs().RunMainQueue();
            const RenderScene& scene = world.Get<RenderScene>();
            REQUIRE_FALSE( scene.mCameras.empty() );
            Required( mGpu->TryCheckNow( [&] {
                mRenderer.Draw(
                    scene, scene.mCameras.front(),
                    RenderTarget{ .mView = mTarget.View(), .mSize = mTarget.Size(), .mFormat = mTarget.Format() } );
            } ) );
            mGpu->Poll();
        }
        return Required( mGpu->TryReadRgba8( mTarget ) );
    }
};

// The color at a pixel of RGBA8 pixels `size` wide, 0 to 1.
inline vec3 ColorAt( const vector<u8>& pixels, i32 size, i32 x, i32 y )
{
    const size_t at = ( static_cast<size_t>( y ) * static_cast<size_t>( size ) + static_cast<size_t>( x ) ) * 4;
    return vec3( pixels[at], pixels[at + 1], pixels[at + 2] ) / 255.0f;
}
}
#endif
