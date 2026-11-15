// The GPU's copies of the registry's models and textures: made at the
// first ask, again after a reload, let go with the asset. Not on the web,
// as gpu_test.cpp.
#ifndef __EMSCRIPTEN__
#include "bubble/render/gpu/gpu_assets.hpp"
#include "render/gpu_helpers.hpp"
#include <doctest.h>

using namespace bubble;

namespace
{
// A catalog, the import cache in memory - by the path an imported file
// has - and a registry that reads models and textures from it.
struct Project
{
    hmap<string, string> mImported;
    AssetCatalog mCatalog;
    Jobs mJobs{ 1 };
    AssetRegistry mAssets{ mCatalog, AssetRegistry::AtOnce( [this]( const AssetId& assetId, string_view kind ) {
                               const auto found = mImported.find( ImportedPath( assetId, kind ).View() );
                               if ( found == mImported.end() )
                                   return expected<string, string>( unexpected( "no such file"s ) );
                               return expected<string, string>( found->second );
                           } ),
                           mJobs };
    Scope<Gpu> mGpu = StartGpu();
    GpuAssets mGpuAssets{ *mGpu, mAssets };

    Project()
    {
        RegisterTextureLoader( mAssets );
        RegisterModelLoader( mAssets );
    }

    // An asset of the project as a scan and the importer would leave it -
    // imported again, if it was.
    AssetPath Import( string_view path, string bytes, string_view kind )
    {
        const AssetPath assetPath = *AssetPath::TryFrom( path );
        auto assetIdMaybe = mCatalog.TryIdOf( assetPath );
        if ( not assetIdMaybe )
        {
            assetIdMaybe = AssetId::Generate();
            mCatalog.Add( *assetIdMaybe, assetPath );
        }
        mImported[ImportedPath( *assetIdMaybe, kind ).String()] = std::move( bytes );
        return assetPath;
    }
};

// One level, `size` texels wide and high, all grey.
string Image( u32 size )
{
    return WriteTextureAsset( TextureAsset{
        .mSrgb = true,
        .mMips = vector<TextureMip>{ TextureMip{
            .mWidth = size, .mHeight = size, .mPixels = vector<u8>( size_t{ size } * size * 4, u8{ 128 } ) } } } );
}

string Triangle()
{
    ModelMesh mesh;
    mesh.mMaterial = "stone";
    mesh.mVertices = vector<ModelVertex>( 3 );
    mesh.mLods = vector<ModelLod>{ ModelLod{ .mError = 0.0f, .mIndices = { 0, 1, 2 } } };
    return WriteModelAsset( ModelAsset{ .mMin = vec3( 0.0f ), .mMax = vec3( 0.0f ), .mMeshes = { mesh } } );
}
}

TEST_CASE( "The GPU's copy of an asset is made once, again after a reload, and let go with the asset" )
{
    Project project;
    const AssetPath brickPath = project.Import( "images/brick.png", Image( 2 ), cTextureKind );
    const AssetPath rockPath = project.Import( "models/rock.glb", Triangle(), cModelKind );
    AssetRef<TextureAsset> brick = project.mAssets.LoadNow<TextureAsset>( brickPath );
    AssetRef<ModelAsset> rock = project.mAssets.LoadNow<ModelAsset>( rockPath );

    const auto textureMaybe = project.mGpuAssets.TryTexture( brick );
    REQUIRE( textureMaybe );
    CHECK( textureMaybe->Size() == ivec2( 2 ) );
    const WGPUTexture made = textureMaybe->Get();
    CHECK( project.mGpuAssets.TryTexture( brick )->Get() == made );
    const auto modelMaybe = project.mGpuAssets.TryModel( rock );
    REQUIRE( modelMaybe );
    REQUIRE( modelMaybe->Meshes().size() == 1 );
    CHECK( modelMaybe->Meshes()[0].mMaterial == "stone" );
    CHECK( project.mGpuAssets.Count() == 2 );

    project.Import( "images/brick.png", Image( 4 ), cTextureKind );
    Required( project.mAssets.TryReload( brickPath ) );
    CHECK( project.mGpuAssets.TryTexture( brick )->Size() == ivec2( 4 ) );

    // Held still: kept.
    project.mGpuAssets.Collect();
    CHECK( project.mGpuAssets.Count() == 2 );
    brick = {};
    project.mGpuAssets.Collect();
    CHECK( project.mGpuAssets.Count() == 1 );
    rock = {};
    project.mGpuAssets.Collect();
    CHECK( project.mGpuAssets.Count() == 0 );
}

TEST_CASE( "An asset still loading, or one that failed, has no GPU copy" )
{
    Project project;
    const AssetPath brickPath = project.Import( "images/brick.png", Image( 2 ), cTextureKind );
    const AssetPath brokenPath = project.Import( "images/broken.png", "not an image", cTextureKind );

    const AssetRef<TextureAsset> loading = project.mAssets.Load<TextureAsset>( brickPath );
    REQUIRE( loading.State() == AssetState::Loading );
    CHECK_FALSE( project.mGpuAssets.TryTexture( loading ) );
    const AssetRef<TextureAsset> broken = project.mAssets.LoadNow<TextureAsset>( brokenPath );
    REQUIRE( broken.State() == AssetState::Failed );
    CHECK_FALSE( project.mGpuAssets.TryTexture( broken ) );
    CHECK( project.mGpuAssets.Count() == 0 );

    project.mJobs.RunMainQueue();
    REQUIRE( loading.Ready() );
    CHECK( project.mGpuAssets.TryTexture( loading ) );
}
#endif
