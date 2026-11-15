// The renderer: a world's RenderScene drawn through a camera - shadow map,
// lit surfaces, tonemapping - into a texture whose pixels come back. Not on
// the web, as gpu_test.cpp.
#ifndef __EMSCRIPTEN__
#include "bubble/assets/model_primitives.hpp"
#include "bubble/render/gpu/renderer.hpp"
#include "bubble/scene/hierarchy.hpp"
#include "bubble/world/transform.hpp"
#include "bubble/world/world.hpp"
#include "render/gpu_helpers.hpp"
#include <doctest.h>

using namespace bubble;

namespace
{
constexpr i32 cSize = 64;
constexpr f32 cHeight = 12.0f;

Text Make( string_view utf8 )
{
    return *Text::TryFrom( utf8 );
}

// A box on a square of ground, a sun to one side and above, and a camera
// high over the box looking straight down: +X to the right, -Z up the
// picture.
struct Drawing
{
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
    GpuTexture mTarget{ *mGpu,
                        GpuTextureOptions{ .mSize = ivec2( cSize ),
                                           .mMips = 1,
                                           .mFormat = WGPUTextureFormat_RGBA8UnormSrgb,
                                           .mUsage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopySrc,
                                           .mLabel = "target" } };

    Drawing()
    {
        RegisterModelLoader( mAssets );
        mApp.AddModule( TransformModule() );
        mApp.AddModule( RenderModule( mAssets ) );
    }

    AssetId Import( string_view path, const ModelAsset& model )
    {
        const AssetId assetId = AssetId::Generate();
        mCatalog.Add( assetId, *AssetPath::TryFrom( path ) );
        mImported[ImportedPath( assetId, cModelKind ).String()] = WriteModelAsset( model );
        return assetId;
    }

    Scene Level()
    {
        Scene scene;
        const Entity ground = CreateChild( scene, scene.Root(), Make( "ground" ) );
        scene.Add<Transform>( ground );
        scene.Add<Model>( ground, Model{ .mAsset = Import( "models/ground.glb", MakePlane( 10.0f ) ) } );
        const Entity box = CreateChild( scene, scene.Root(), Make( "box" ) );
        scene.Add<Transform>( box, Transform{ .mPosition = { 0.0f, 0.5f, 0.0f } } );
        scene.Add<Model>( box, Model{ .mAsset = Import( "models/box.glb", MakeBox( vec3( 1.0f ) ) ) } );
        const Entity sun = CreateChild( scene, scene.Root(), Make( "sun" ) );
        scene.Add<Transform>( sun, Transform{ .mRotation = { glm::radians( -50.0f ), glm::radians( 30.0f ), 0.0f } } );
        scene.Add<DirectionalLight>( sun );
        const Entity eye = CreateChild( scene, scene.Root(), Make( "eye" ) );
        scene.Add<Transform>( eye, Transform{ .mPosition = { 0.0f, cHeight, 0.0f },
                                              .mRotation = { glm::radians( -90.0f ), 0.0f, 0.0f } } );
        scene.Add<Camera>( eye );
        return scene;
    }

    // The world's first frame - its models loaded - through its camera.
    vector<u8> Draw( World& world )
    {
        world.Frame( 1.0 / 60.0 );
        mApp.GetJobs().RunMainQueue();
        world.Frame( 1.0 / 60.0 );
        const RenderScene& scene = world.Get<RenderScene>();
        REQUIRE( scene.mCameras.size() == 1 );
        Required( mGpu->TryCheckNow( [&] {
            mRenderer.Draw(
                scene, scene.mCameras[0],
                RenderTarget{ .mView = mTarget.View(), .mSize = mTarget.Size(), .mFormat = mTarget.Format() } );
        } ) );
        return Required( mGpu->TryReadRgba8( mTarget ) );
    }
};

// Where a point of the ground (or `height` above it) is in the picture.
ivec2 PixelOf( f32 x, f32 z, f32 height = 0.0f )
{
    const f32 reach = ( cHeight - height ) * tan( Camera().mFov * 0.5f );
    const f32 ndcX = x / reach;
    const f32 ndcY = -z / reach;
    return ivec2( static_cast<i32>( ( ndcX + 1.0f ) * 0.5f * cSize ),
                  static_cast<i32>( ( 1.0f - ndcY ) * 0.5f * cSize ) );
}

vec3 ColorAt( const vector<u8>& pixels, ivec2 pixel )
{
    const size_t at = ( static_cast<size_t>( pixel.y ) * cSize + static_cast<size_t>( pixel.x ) ) * 4;
    return vec3( pixels[at], pixels[at + 1], pixels[at + 2] ) / 255.0f;
}

f32 Brightness( vec3 color )
{
    return glm::dot( color, vec3( 0.2126f, 0.7152f, 0.0722f ) );
}
}

TEST_CASE( "A box under the sun is lit on top and shades the ground beside it" )
{
    Drawing drawing;
    World world( drawing.mApp, drawing.Level() );
    const vector<u8> pixels = drawing.Draw( world );

    // Past the ground's edge: the sky behind it.
    const vec3 sky = ColorAt( pixels, ivec2( 1, 1 ) );
    CHECK( sky.b > sky.r );
    // The sun shines down and away, to -X and -Z: the box's shadow falls
    // that way, and the ground the other way is in the sun.
    const vec3 top = ColorAt( pixels, PixelOf( 0.0f, 0.0f, 1.0f ) );
    const vec3 sunny = ColorAt( pixels, PixelOf( 1.5f, 1.5f ) );
    const vec3 shaded = ColorAt( pixels, PixelOf( -0.6f, -1.0f ) );
    CAPTURE( top );
    CAPTURE( sunny );
    CAPTURE( shaded );
    CHECK( Brightness( sunny ) > 2.0f * Brightness( shaded ) );
    CHECK( Brightness( top ) > 2.0f * Brightness( shaded ) );
    // Not black either: the sky's light reaches the shade.
    CHECK( Brightness( shaded ) > 0.05f );
}
#endif
