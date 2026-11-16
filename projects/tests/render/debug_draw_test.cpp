// The debug drawing - lines and shapes of them, for one frame - and finding
// the entity drawn at a pixel. The drawing's own tests need no GPU; what
// shows on screen and the picking are not on the web, as gpu_test.cpp.
#include "bubble/render/debug_draw.hpp"
#include "bubble/render/render_scene.hpp"
#include "bubble/world/transform.hpp"
#include "bubble/world/world.hpp"
#include "render/render_fixture.hpp"
#include <doctest.h>

using namespace bubble;

namespace
{
const Color cRed{ .mR = 1.0f, .mG = 0.0f, .mB = 0.0f, .mA = 1.0f };
}

TEST_CASE( "A box is twelve lines, a sphere three circles, axes three colors, and a take starts over" )
{
    DebugDraw draw;
    draw.Box( vec3( -1.0f ), vec3( 1.0f ), cRed );
    CHECK( draw.Count() == 12 );
    draw.Sphere( vec3( 0.0f ), 2.0f, cRed );
    CHECK( draw.Count() == 12 + 3 * 24 );
    draw.Axes( mat4( 1.0f ), 1.0f );
    const vector<DebugLine> lines = draw.Take();
    REQUIRE( lines.size() == 12 + 3 * 24 + 3 );
    CHECK( draw.Count() == 0 );
    // Every edge of the box is two units long and runs along an axis.
    for ( size_t at = 0; at < 12; ++at )
        CHECK( glm::length( lines[at].mTo - lines[at].mFrom ) == doctest::Approx( 2.0f ) );
    // Every point of the sphere is on it.
    for ( size_t at = 12; at < 12 + 3 * 24; ++at )
        CHECK( glm::length( lines[at].mFrom ) == doctest::Approx( 2.0f ) );
    const DebugLine& y = lines[lines.size() - 2];
    CHECK( y.mTo == vec3( 0.0f, 1.0f, 0.0f ) );
    CHECK( y.mColor == Color{ .mR = 0.0f, .mG = 1.0f, .mB = 0.0f, .mA = 1.0f } );
}

TEST_CASE( "What a world draws shows for the frame it was drawn in" )
{
    AssetCatalog catalog;
    App app( AppOptions{ .mThreads = 1 } );
    AssetRegistry assets(
        catalog, AssetRegistry::AtOnce( []( const AssetId&, string_view ) { return expected<string, string>( ""s ); } ),
        app.GetJobs() );
    app.AddModule( TransformModule() );
    app.AddModule( RenderModule( assets ) );
    World world( app, Scene() );
    bool drawing = true;
    world.AddStep( Phase::Update, [&]( f32 ) {
        if ( drawing )
            world.Get<DebugDraw>().Line( vec3( 0.0f ), vec3( 1.0f ), cRed );
    } );

    world.Frame( 1.0 / 60.0 );
    REQUIRE( world.Get<RenderScene>().mLines.size() == 1 );
    CHECK( world.Get<RenderScene>().mLines[0].mTo == vec3( 1.0f ) );
    drawing = false;
    world.Frame( 1.0 / 60.0 );
    CHECK( world.Get<RenderScene>().mLines.empty() );
}

#ifndef __EMSCRIPTEN__
namespace
{
constexpr i32 cSize = 64;
constexpr f32 cHeight = 12.0f;

// Where a point of the ground is in the picture of a camera high above,
// looking straight down: +X to the right, -Z up.
ivec2 PixelOf( f32 x, f32 z, f32 height = 0.0f )
{
    const f32 reach = ( cHeight - height ) * tan( Camera().mFov * 0.5f );
    return ivec2( static_cast<i32>( ( x / reach + 1.0f ) * 0.5f * cSize ),
                  static_cast<i32>( ( z / reach + 1.0f ) * 0.5f * cSize ) );
}

struct Picking : test::RenderFixture
{
    AssetId mBox = ImportModel( "models/box.glb", MakeBox( vec3( 1.0f ) ) );
    Entity mLeft;
    Entity mRight;

    // Two boxes apart under a camera high above; no ground, the sky
    // between them.
    Scene Level()
    {
        Scene scene;
        for ( const auto& [name, x] :
              { pair<const char*, f32>{ "left", -2.0f }, pair<const char*, f32>{ "right", 2.0f } } )
        {
            const Entity box = CreateChild( scene, scene.Root(), test::Make( name ) );
            scene.Add<Transform>( box, Transform{ .mPosition = { x, 0.5f, 0.0f } } );
            scene.Add<Model>( box, Model{ .mAsset = mBox, .mMaterials = {}, .mTint = {} } );
            ( x < 0 ? mLeft : mRight ) = box;
        }
        const Entity eye = CreateChild( scene, scene.Root(), test::Make( "eye" ) );
        scene.Add<Transform>( eye, Transform{ .mPosition = { 0.0f, cHeight, 0.0f },
                                              .mRotation = { glm::radians( -90.0f ), 0.0f, 0.0f } } );
        scene.Add<Camera>( eye );
        return scene;
    }

    // Frames until the pick is back: a slow GPU answers frames later.
    opt<Entity> PickAt( World& world, ivec2 pixel )
    {
        mView.Pick( pixel );
        for ( u32 frame = 0; frame < 600; ++frame )
        {
            Draw( world, 1 );
            if ( const auto pickedMaybe = mView.TryPicked() )
                return pickedMaybe;
        }
        return nullopt;
    }
};
}

TEST_CASE( "A click finds the entity drawn where it is, and nothing where nothing is" )
{
    Picking f;
    World world( f.mApp, f.Level() );
    f.Draw( world );
    CHECK_FALSE( f.mView.TryPicked() );

    CHECK( f.PickAt( world, PixelOf( -2.0f, 0.0f, 1.0f ) ) == f.mLeft );
    // Given once.
    CHECK_FALSE( f.mView.TryPicked() );
    CHECK( f.PickAt( world, PixelOf( 2.0f, 0.0f, 1.0f ) ) == f.mRight );
    CHECK( f.PickAt( world, PixelOf( 0.0f, 0.0f ) ) == Entity::Null );
    // Past the target's edge: its nearest pixel.
    CHECK( f.PickAt( world, ivec2( -10, cSize / 2 ) ) == Entity::Null );
}

TEST_CASE( "The debug drawing's lines show over the sky" )
{
    Picking f;
    World world( f.mApp, f.Level() );
    // Drawn again each frame: a line lasts the frame it was drawn in.
    world.AddStep( Phase::Update, [&]( f32 ) {
        world.Get<DebugDraw>().Line( vec3( -6.0f, 0.0f, -5.0f ), vec3( 6.0f, 0.0f, -5.0f ), cRed );
    } );
    const vector<u8> pixels = f.Draw( world );
    const ivec2 onLine = PixelOf( 0.0f, -5.0f );
    const vec3 line = test::ColorAt( pixels, cSize, onLine.x, onLine.y );
    const vec3 sky = test::ColorAt( pixels, cSize, onLine.x, onLine.y + 4 );
    CAPTURE( line.r );
    CAPTURE( line.b );
    CHECK( line.r > line.b + 0.2f );
    CHECK( sky.b > sky.r );
}
#endif
