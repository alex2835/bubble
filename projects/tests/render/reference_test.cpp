// The renderer against a picture kept in the repository: a scene of every
// kind of thing it draws - the standard material's colors, a variant, a
// metal, glass, a material's own WGSL, the sun's shadow, a lamp, the debug
// drawing - drawn as it was when the picture was taken. What a rasterizer
// may round differently passes; anything more is a change to look at
// (render/reference_image.hpp). The drawing is not on the web, as
// gpu_test.cpp; the pictures' own test is.
#include "render/reference_image.hpp"
#include <doctest.h>

using namespace bubble;
using namespace bubble::test;

TEST_CASE( "A picture written as PNG reads back the same, and a difference is measured in levels" )
{
    vector<u8> pixels;
    for ( u32 at = 0; at < 300 * 200; ++at )
        for ( const u32 channel : { at % 256, ( at / 7 ) % 256, ( at * 13 ) % 256, 255u } )
            pixels.push_back( static_cast<u8>( channel ) );
    // Larger than one of zlib's stored blocks.
    const string png = EncodePng( pixels, ivec2( 300, 200 ) );
    i32 width = 0;
    i32 height = 0;
    i32 channels = 0;
    u8* decoded = stbi_load_from_memory( reinterpret_cast<const u8*>( png.data() ), static_cast<i32>( png.size() ),
                                         &width, &height, &channels, 4 );
    REQUIRE( decoded != nullptr );
    const vector<u8> back( decoded, decoded + pixels.size() );
    stbi_image_free( decoded );
    CHECK( ivec2( width, height ) == ivec2( 300, 200 ) );
    CHECK( back == pixels );

    vector<u8> brighter = pixels;
    brighter[0] = static_cast<u8>( brighter[0] < 200 ? brighter[0] + 30 : brighter[0] - 30 );
    const ImageDifference difference = Compare( pixels, brighter, 24 );
    CHECK( difference.mMean == doctest::Approx( 30.0 / ( 300 * 200 * 3 ) ) );
    CHECK( difference.mFarShare == doctest::Approx( 1.0 / ( 300 * 200 ) ) );
}

#ifndef __EMSCRIPTEN__
#include "render/render_fixture.hpp"

namespace
{
constexpr ivec2 cSize{ 160, 90 };

constexpr string_view cPulse = R"wgsl(struct Material
{
    glow: vec3f, // @color @default( 0.2, 1, 0.6 )
    rate: f32, // @default( 2 )
}
@group( 2 ) @binding( 0 ) var<uniform> material: Material;

fn surface( in: SurfaceInput ) -> SurfaceOutput
{
    var out = surface_defaults( in );
    out.albedo = vec3f( 0.05 );
    out.emissive = material.glow * ( 1.0 + sin( in.time * material.rate ) );
    return out;
}
)wgsl";

Scene Level( RenderFixture& f )
{
    const AssetId paint =
        f.ImportMaterial( "materials/paint.mat", R"({ "params": { "base_color": [ 0.8, 0.15, 0.1, 1 ] } })" );
    const AssetId shiny = f.ImportMaterial(
        "materials/shiny_paint.mat",
        format( R"({{ "base": {{ "asset": "{}" }}, "params": {{ "roughness": 0.2 }} }})", paint.ToString() ) );
    const AssetId gold =
        f.ImportMaterial( "materials/gold.mat",
                          R"({ "params": { "base_color": [ 1, 0.78, 0.35, 1 ], "metallic": 1, "roughness": 0.3 } })" );
    const AssetId glass = f.ImportMaterial(
        "materials/glass.mat",
        R"({ "transparent": true, "params": { "base_color": [ 0.6, 0.8, 1, 0.35 ], "roughness": 0.1 } })" );
    const AssetId pulse = f.ImportMaterial(
        "materials/pulse.mat", format( R"({{ "shader": {{ "asset": "{}" }} }})",
                                       f.Import( "shaders/pulse.wgsl", string( cPulse ), cShaderKind ).ToString() ) );
    const AssetId grass = f.ImportMaterial(
        "materials/grass.mat", R"({ "params": { "base_color": [ 0.45, 0.6, 0.35, 1 ], "roughness": 0.9 } })" );
    const AssetId box = f.ImportModel( "models/box.glb", MakeBox( vec3( 1.0f ) ) );

    Scene scene;
    const Entity ground = CreateChild( scene, scene.Root(), Make( "ground" ) );
    scene.Add<Transform>( ground );
    scene.Add<Model>( ground, Model{ .mAsset = f.ImportModel( "models/ground.glb", MakePlane( 20.0f ) ),
                                     .mMaterials = { grass },
                                     .mTint = {} } );
    const array<pair<Transform, AssetId>, 5> boxes{
        pair<Transform, AssetId>{ Transform{ .mPosition = { 0.0f, 0.5f, 0.0f } }, paint },
        pair<Transform, AssetId>{
            Transform{ .mPosition = { 2.5f, 1.0f, -1.0f }, .mRotation = { 0.0f, 0.6f, 0.0f }, .mScale = vec3( 2.0f ) },
            gold },
        pair<Transform, AssetId>{
            Transform{ .mPosition = { -2.0f, 0.35f, 1.5f }, .mRotation = { 0.0f, 0.3f, 0.0f }, .mScale = vec3( 0.7f ) },
            glass },
        pair<Transform, AssetId>{ Transform{ .mPosition = { -1.5f, 1.5f, -2.5f }, .mRotation = { 0.5f, 0.8f, 0.2f } },
                                  pulse },
        pair<Transform, AssetId>{ Transform{ .mPosition = { 1.0f, 0.25f, 2.5f }, .mScale = { 3.0f, 0.5f, 0.5f } },
                                  shiny },
    };
    for ( const auto& [placed, material] : boxes )
    {
        const Entity entity = CreateChild( scene, scene.Root(), Make( "box" ) );
        scene.Add<Transform>( entity, placed );
        scene.Add<Model>( entity, Model{ .mAsset = box, .mMaterials = { material }, .mTint = {} } );
    }
    const Entity sun = CreateChild( scene, scene.Root(), Make( "sun" ) );
    scene.Add<Transform>( sun, Transform{ .mRotation = { glm::radians( -40.0f ), glm::radians( 35.0f ), 0.0f } } );
    scene.Add<DirectionalLight>( sun );
    const Entity lamp = CreateChild( scene, scene.Root(), Make( "lamp" ) );
    scene.Add<Transform>( lamp, Transform{ .mPosition = { -2.0f, 1.5f, 3.0f } } );
    scene.Add<PointLight>( lamp, PointLight{ .mColor = { .mR = 1.0f, .mG = 0.6f, .mB = 0.3f, .mA = 1.0f },
                                             .mIntensity = 15.0f,
                                             .mRange = 8.0f } );
    const Entity eye = CreateChild( scene, scene.Root(), Make( "eye" ) );
    const f32 turn = 2.2f;
    const f32 pitch = -0.55f;
    scene.Add<Transform>(
        eye,
        Transform{ .mPosition = vec3( sin( turn ) * cos( pitch ), -sin( pitch ), cos( turn ) * cos( pitch ) ) * 11.0f,
                   .mRotation = { pitch, turn, 0.0f } } );
    scene.Add<Camera>( eye );
    return scene;
}
}

TEST_CASE( "The renderer draws its scene as the reference picture has it" )
{
    RenderFixture f( cSize );
    World world( f.mApp, Level( f ) );
    world.AddStep( Phase::Update, [&]( f32 ) { world.Get<DebugDraw>().Axes( mat4( 1.0f ), 1.5f ); } );
    const vector<u8> pixels = f.Draw( world );
    CheckAgainstReference( pixels, cSize, "lit_scene" );
}
#endif
