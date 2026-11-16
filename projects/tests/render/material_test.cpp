// Materials drawn: a box on the ground seen from straight above, its top
// face the color its material makes - the standard one's values, a base's
// that a variant does not set, its own WGSL's, a texture's, a tint's - the
// ground through a transparent one, and the placeholder for one that cannot
// be drawn. Not on the web, as gpu_test.cpp.
#ifndef __EMSCRIPTEN__
#include "bubble/assets/model_primitives.hpp"
#include "bubble/assets/shader_asset.hpp"
#include "bubble/render/gpu/renderer.hpp"
#include "bubble/scene/hierarchy.hpp"
#include "bubble/world/transform.hpp"
#include "bubble/world/world.hpp"
#include "log_watch.hpp"
#include "render/gpu_helpers.hpp"
#include <doctest.h>

using namespace bubble;

namespace
{
constexpr i32 cSize = 64;

Text Make( string_view utf8 )
{
    return *Text::TryFrom( utf8 );
}

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
    AssetId mBox;
    AssetId mGround;

    Drawing()
    {
        RegisterModelLoader( mAssets );
        RegisterMaterialLoader( mAssets );
        RegisterShaderLoader( mAssets );
        RegisterTextureLoader( mAssets );
        mApp.AddModule( TransformModule() );
        mApp.AddModule( RenderModule( mAssets ) );
        mBox = Import( "models/box.glb", WriteModelAsset( MakeBox( vec3( 1.0f ) ) ), cModelKind );
        mGround = Import( "models/ground.glb", WriteModelAsset( MakePlane( 10.0f ) ), cModelKind );
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

    AssetId Material( string_view path, const string& json )
    {
        const auto materialMaybe = TryReadMaterialAsset( json );
        REQUIRE_MESSAGE( materialMaybe, materialMaybe.error() );
        return Import( path, WriteMaterialAsset( *materialMaybe ), cMaterialKind );
    }

    // The box with `materials` and `tint` on the ground with its own, under
    // the sun, a camera high above looking straight down.
    Scene Level( vector<AssetId> materials, Color tint = {}, vector<AssetId> groundMaterials = {} )
    {
        Scene scene;
        const Entity ground = CreateChild( scene, scene.Root(), Make( "ground" ) );
        scene.Add<Transform>( ground );
        scene.Add<Model>( ground, Model{ .mAsset = mGround, .mMaterials = std::move( groundMaterials ), .mTint = {} } );
        const Entity box = CreateChild( scene, scene.Root(), Make( "box" ) );
        scene.Add<Transform>( box, Transform{ .mPosition = { 0.0f, 0.5f, 0.0f } } );
        scene.Add<Model>( box, Model{ .mAsset = mBox, .mMaterials = std::move( materials ), .mTint = tint } );
        const Entity sun = CreateChild( scene, scene.Root(), Make( "sun" ) );
        scene.Add<Transform>( sun, Transform{ .mRotation = { glm::radians( -50.0f ), glm::radians( 30.0f ), 0.0f } } );
        scene.Add<DirectionalLight>( sun );
        const Entity eye = CreateChild( scene, scene.Root(), Make( "eye" ) );
        scene.Add<Transform>(
            eye, Transform{ .mPosition = { 0.0f, 12.0f, 0.0f }, .mRotation = { glm::radians( -90.0f ), 0.0f, 0.0f } } );
        scene.Add<Camera>( eye );
        return scene;
    }

    // A few frames - long enough for loads, the GPU's checks and textures to
    // come - and the last one's pixels.
    vector<u8> Draw( World& world )
    {
        for ( u32 frame = 0; frame < 8; ++frame )
        {
            world.Frame( 1.0 / 60.0 );
            mApp.GetJobs().RunMainQueue();
            const RenderScene& scene = world.Get<RenderScene>();
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

vec3 ColorAt( const vector<u8>& pixels, i32 x, i32 y = cSize / 2 )
{
    const size_t at = ( static_cast<size_t>( y ) * cSize + static_cast<size_t>( x ) ) * 4;
    return vec3( pixels[at], pixels[at + 1], pixels[at + 2] ) / 255.0f;
}

// The box's top, seen from above: the middle of the picture.
vec3 Top( const vector<u8>& pixels )
{
    return ColorAt( pixels, cSize / 2 );
}

bool Mostly( vec3 color, u32 channel )
{
    for ( u32 other = 0; other < 3; ++other )
        if ( other != channel and color[channel] < 2.0f * color[other] + 0.05f )
            return false;
    return true;
}

constexpr u32 cRed = 0;
constexpr u32 cGreen = 1;
constexpr u32 cBlue = 2;
}

TEST_CASE( "A mesh draws with its material, and a variant takes from its base what it does not set" )
{
    Drawing drawing;
    const AssetId base = drawing.Material( "materials/paint.mat",
                                           R"({ "params": { "base_color": [ 1, 0, 0, 1 ], "roughness": 0.9 } })" );
    const AssetId variant = drawing.Material(
        "materials/shiny_paint.mat",
        format( R"({{ "base": {{ "asset": "{}" }}, "params": {{ "roughness": 0.3 }} }})", base.ToString() ) );
    World world( drawing.mApp, drawing.Level( { variant } ) );
    const vec3 red = Top( drawing.Draw( world ) );
    CAPTURE( red.r );
    CAPTURE( red.g );
    CHECK( Mostly( red, cRed ) );

    // The base painted again: the variant follows.
    drawing.Material( "materials/paint.mat", R"({ "params": { "base_color": [ 0, 0, 1, 1 ], "roughness": 0.9 } })" );
    Required( drawing.mAssets.TryReload( *AssetPath::TryFrom( "materials/paint.mat" ) ) );
    const vec3 blue = Top( drawing.Draw( world ) );
    CAPTURE( blue.b );
    CHECK( Mostly( blue, cBlue ) );
}

TEST_CASE( "A material's own WGSL makes its surface, a parameter it does not set its default" )
{
    Drawing drawing;
    const AssetId shader = drawing.Import( "shaders/glow.wgsl", R"wgsl(struct Material
{
    glow: vec4f, // @color @default( 0, 1, 0, 1 )
}
@group( 2 ) @binding( 0 ) var<uniform> material: Material;

fn surface( in: SurfaceInput ) -> SurfaceOutput
{
    var out = surface_defaults( in );
    out.albedo = vec3f( 0.0 );
    out.emissive = material.glow.rgb;
    return out;
}
)wgsl",
                                           cShaderKind );
    const AssetId glow =
        drawing.Material( "materials/glow.mat", format( R"({{ "shader": {{ "asset": "{}" }} }})", shader.ToString() ) );
    World world( drawing.mApp, drawing.Level( { glow } ) );
    const vec3 green = Top( drawing.Draw( world ) );
    CAPTURE( green.g );
    CHECK( Mostly( green, cGreen ) );
}

TEST_CASE( "A texture parameter is sampled, and a model's tint colors what it draws" )
{
    Drawing drawing;
    // Red on the left, blue on the right.
    const AssetId image = drawing.Import(
        "images/halves.png",
        WriteTextureAsset( TextureAsset{
            .mSrgb = true,
            .mMips = { TextureMip{ .mWidth = 2, .mHeight = 1, .mPixels = { 255, 0, 0, 255, 0, 0, 255, 255 } } } } ),
        cTextureKind );
    const AssetId halves = drawing.Material(
        "materials/halves.mat",
        format( R"({{ "params": {{ "base_color_map": {{ "asset": "{}" }} }} }})", image.ToString() ) );
    World world( drawing.mApp, drawing.Level( { halves } ) );
    const vector<u8> pixels = drawing.Draw( world );
    // Towards the top's left and right edges, seen from above: +X to the
    // right. The two texels blend between them, and across the edge too.
    CHECK( Mostly( ColorAt( pixels, cSize / 2 - 2 ), cRed ) );
    CHECK( Mostly( ColorAt( pixels, cSize / 2 + 1 ), cBlue ) );

    World tinted( drawing.mApp, drawing.Level( {}, Color{ .mR = 0.0f, .mG = 1.0f, .mB = 0.0f, .mA = 1.0f } ) );
    CHECK( Mostly( Top( drawing.Draw( tinted ) ), cGreen ) );
}

TEST_CASE( "A transparent material shows what is behind it" )
{
    Drawing drawing;
    const AssetId red = drawing.Material( "materials/red.mat", R"({ "params": { "base_color": [ 1, 0, 0, 1 ] } })" );
    const AssetId glass = drawing.Material(
        "materials/glass.mat", R"({ "transparent": true, "params": { "base_color": [ 0, 0, 1, 0.5 ] } })" );
    World world( drawing.mApp, drawing.Level( { glass }, {}, { red } ) );
    const vec3 top = Top( drawing.Draw( world ) );
    CAPTURE( top.r );
    CAPTURE( top.b );
    // The red ground under the blue glass, both.
    CHECK( top.r > top.g + 0.1f );
    CHECK( top.b > top.g + 0.1f );
}

TEST_CASE( "A material that cannot be drawn is the placeholder, and the log says why once" )
{
    Drawing drawing;
    const AssetId broken = drawing.Import( "shaders/broken.wgsl", R"wgsl(struct Material
{
    glow: vec4f, // @color
}
@group( 2 ) @binding( 0 ) var<uniform> material: Material;

fn surface( in: SurfaceInput ) -> SurfaceOutput
{
    var out = surface_defaults( in );
    out.emissive = material.glow.rgb * nothing;
    return out;
}
)wgsl",
                                           cShaderKind );
    const AssetId uncompiled = drawing.Material(
        "materials/uncompiled.mat", format( R"({{ "shader": {{ "asset": "{}" }} }})", broken.ToString() ) );
    const AssetId shiny = drawing.Material( "materials/shiny.mat", R"({ "params": { "shine": 1 } })" );
    const test::LogWatch log;
    World world( drawing.mApp, drawing.Level( { uncompiled } ) );
    const vec3 placeholder = Top( drawing.Draw( world ) );
    CAPTURE( placeholder.r );
    CAPTURE( placeholder.g );
    CHECK( placeholder.r > 0.5f );
    CHECK( placeholder.b > 0.5f );
    CHECK( placeholder.g < 0.2f );
    CHECK( log.Saw( LogLevel::Error, { "shaders/broken.wgsl", "nothing", "wgsl:10:" } ) );
    size_t said = 0;
    for ( const LogEntry& entry : log.Entries() )
        said += entry.mText.find( "nothing" ) != string::npos ? 1 : 0;
    CHECK( said == 1 );

    World other( drawing.mApp, drawing.Level( { shiny } ) );
    CHECK( Top( drawing.Draw( other ) ).g < 0.2f );
    CHECK( log.Saw( LogLevel::Error,
                    { "materials/shiny.mat: params.shine: standard_material.wgsl has no such parameter (parameters: "
                      "base_color, emissive, metallic, roughness" } ) );
}
#endif
