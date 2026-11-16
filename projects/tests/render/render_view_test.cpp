// Views: the game's, an editor's and a preview's, drawn in one frame - two
// of one world, one of another, each through its own camera into its own
// target - and none disturbs the others: each its own levels of detail, its
// own pick, its own pixels. Not on the web, as gpu_test.cpp.
#ifndef __EMSCRIPTEN__
#include "render/render_fixture.hpp"
#include <doctest.h>

using namespace bubble;
using namespace bubble::test;

namespace
{
// A square two across facing +Z: both its triangles, then one, half a unit
// off at most.
ModelAsset Square()
{
    ModelMesh mesh;
    mesh.mMaterial = "default";
    mesh.mVertices = vector<ModelVertex>{
        ModelVertex{ .mPosition = { -1.0f, -1.0f, 0.0f }, .mNormal = { 0.0f, 0.0f, 1.0f }, .mUv = { 0.0f, 1.0f } },
        ModelVertex{ .mPosition = { 1.0f, -1.0f, 0.0f }, .mNormal = { 0.0f, 0.0f, 1.0f }, .mUv = { 1.0f, 1.0f } },
        ModelVertex{ .mPosition = { 1.0f, 1.0f, 0.0f }, .mNormal = { 0.0f, 0.0f, 1.0f }, .mUv = { 1.0f, 0.0f } },
        ModelVertex{ .mPosition = { -1.0f, 1.0f, 0.0f }, .mNormal = { 0.0f, 0.0f, 1.0f }, .mUv = { 0.0f, 0.0f } },
    };
    mesh.mLods = vector<ModelLod>{
        ModelLod{ .mError = 0.0f, .mIndices = { 0, 1, 2, 0, 2, 3 } },
        ModelLod{ .mError = 0.5f, .mIndices = { 0, 1, 2 } },
    };
    return ModelAsset{ .mMin = { -1.0f, -1.0f, 0.0f }, .mMax = { 1.0f, 1.0f, 0.0f }, .mMeshes = { mesh } };
}

GpuTexture Target( const Gpu& gpu, i32 size )
{
    return GpuTexture( gpu, GpuTextureOptions{ .mSize = ivec2( size ),
                                               .mMips = 1,
                                               .mFormat = WGPUTextureFormat_RGBA8UnormSrgb,
                                               .mUsage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopySrc,
                                               .mLabel = "target" } );
}

RenderTarget TargetOf( const GpuTexture& texture )
{
    return RenderTarget{ .mView = texture.View(), .mSize = texture.Size(), .mFormat = texture.Format() };
}

Entity Add( Scene& scene, string_view name, Transform placed )
{
    const Entity entity = CreateChild( scene, scene.Root(), Make( name ) );
    scene.Add<Transform>( entity, placed );
    return entity;
}
}

TEST_CASE( "Three views in one frame - two of one world, one of another - each draws and picks its own" )
{
    RenderFixture f( 64 );
    const AssetId box = f.ImportModel( "models/box.glb", MakeBox( vec3( 1.0f ) ) );
    const AssetId square = f.ImportModel( "models/square.glb", Square() );
    const AssetId red = f.ImportMaterial( "materials/red.mat", R"({ "params": { "base_color": [ 1, 0, 0, 1 ] } })" );

    // The level: a box, a square behind it, a sun, the game's camera high
    // above.
    Scene level;
    const Entity levelBox = Add( level, "box", Transform{ .mPosition = { 0.0f, 0.5f, 0.0f } } );
    level.Add<Model>( levelBox, Model{ .mAsset = box, .mMaterials = {}, .mTint = {} } );
    const Entity sign = Add( level, "sign", Transform{ .mPosition = { 0.0f, 1.0f, -3.0f } } );
    level.Add<Model>( sign, Model{ .mAsset = square, .mMaterials = {}, .mTint = {} } );
    level.Add<DirectionalLight>(
        Add( level, "sun", Transform{ .mRotation = { glm::radians( -60.0f ), glm::radians( 20.0f ), 0.0f } } ) );
    level.Add<Camera>(
        Add( level, "eye",
             Transform{ .mPosition = { 0.0f, 12.0f, 0.0f }, .mRotation = { glm::radians( -90.0f ), 0.0f, 0.0f } } ) );
    World world( f.mApp, std::move( level ) );

    // A material's preview: a red box of its own, its own camera.
    Scene shelf;
    const Entity sample = Add( shelf, "sample", Transform{ .mPosition = { 0.0f, 0.0f, 0.0f } } );
    shelf.Add<Model>( sample, Model{ .mAsset = box, .mMaterials = { red }, .mTint = {} } );
    shelf.Add<DirectionalLight>(
        Add( shelf, "sun", Transform{ .mRotation = { glm::radians( -30.0f ), 0.0f, 0.0f } } ) );
    shelf.Add<Camera>( Add( shelf, "eye", Transform{ .mPosition = { 0.0f, 0.0f, 3.0f } } ) );
    World preview( f.mApp, std::move( shelf ) );

    // The editor's camera is its own, no entity's: far back from the level.
    const RenderCamera editorCamera{ .mWorld = glm::translate( mat4( 1.0f ), vec3( 0.0f, 0.5f, 60.0f ) ) };
    RenderView game;
    RenderView editor;
    RenderView look;
    const GpuTexture gameTarget = Target( *f.mGpu, 64 );
    const GpuTexture editorTarget = Target( *f.mGpu, 48 );
    const GpuTexture previewTarget = Target( *f.mGpu, 32 );

    // Both asked in the same frame.
    game.Pick( ivec2( 32, 32 ) );
    look.Pick( ivec2( 16, 16 ) );
    opt<Entity> gamePicked;
    opt<Entity> previewPicked;
    // Frames enough for everything to load, and until the picks are back:
    // a slow GPU - a software one - answers frames later than it was asked.
    for ( u32 frame = 0; frame < 8 or ( ( not gamePicked or not previewPicked ) and frame < 600 ); ++frame )
    {
        world.Frame( 1.0 / 60.0 );
        preview.Frame( 1.0 / 60.0 );
        f.mApp.GetJobs().RunMainQueue();
        Required( f.mGpu->TryCheckNow( [&] {
            const RenderScene& level = world.Get<RenderScene>();
            const RenderScene& previewed = preview.Get<RenderScene>();
            f.mRenderer.Draw( game, level, level.mCameras.front(), TargetOf( gameTarget ) );
            f.mRenderer.Draw( editor, level, editorCamera, TargetOf( editorTarget ) );
            f.mRenderer.Draw( look, previewed, previewed.mCameras.front(), TargetOf( previewTarget ) );
        } ) );
        f.mGpu->Poll();
        if ( const auto pickedMaybe = game.TryPicked() )
            gamePicked = pickedMaybe;
        if ( const auto pickedMaybe = look.TryPicked() )
            previewPicked = pickedMaybe;
    }

    // Each view its own levels: the game's camera near the square draws
    // all of it, the editor's far one half.
    CHECK( game.Stats().mTriangles == 12 + 2 );
    CHECK( editor.Stats().mTriangles == 12 + 1 );
    CHECK( look.Stats().mTriangles == 12 );
    // Each its own pick, each of its own world.
    CHECK( gamePicked == levelBox );
    CHECK( previewPicked == sample );
    // Each its own pixels: grey from above, red in the preview.
    const vector<u8> gamePixels = Required( f.mGpu->TryReadRgba8( gameTarget ) );
    const vector<u8> previewPixels = Required( f.mGpu->TryReadRgba8( previewTarget ) );
    const vec3 top = ColorAt( gamePixels, 64, 32, 32 );
    const vec3 sampled = ColorAt( previewPixels, 32, 16, 16 );
    CHECK( abs( top.r - top.b ) < 0.1f );
    CHECK( sampled.r > 2.0f * sampled.g + 0.05f );
    CHECK( editor.Stats().mVisible == 2 );
}
#endif
