// What the renderer chooses to draw: nothing out of the camera's view, meshes
// alike in one call - each instance where its own item is - and each mesh at
// the level of detail its size on screen needs, kept at the edge so it does
// not flicker. Not on the web, as gpu_test.cpp.
#ifndef __EMSCRIPTEN__
#include "render/render_fixture.hpp"
#include <doctest.h>

using namespace bubble;
using namespace bubble::test;

namespace
{
constexpr i32 cSize = 64;

void AddCamera( Scene& scene, Transform placed )
{
    const Entity eye = CreateChild( scene, scene.Root(), Make( "eye" ) );
    scene.Add<Transform>( eye, placed );
    scene.Add<Camera>( eye );
}

void AddSun( Scene& scene )
{
    const Entity sun = CreateChild( scene, scene.Root(), Make( "sun" ) );
    scene.Add<Transform>( sun, Transform{ .mRotation = { glm::radians( -60.0f ), glm::radians( 20.0f ), 0.0f } } );
    scene.Add<DirectionalLight>( sun );
}

Entity AddModel( Scene& scene, string_view name, const AssetId& model, vec3 position, vector<AssetId> materials = {} )
{
    const Entity entity = CreateChild( scene, scene.Root(), Make( name ) );
    scene.Add<Transform>( entity, Transform{ .mPosition = position } );
    scene.Add<Model>( entity, Model{ .mAsset = model, .mMaterials = std::move( materials ), .mTint = {} } );
    return entity;
}

// A square two across facing +Z: both its triangles, then one - half of
// it, half a unit off at most.
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

// Where the square's error is this many pixels on a 64-pixel target with
// the default camera, looking down -Z from the origin: its sphere's
// surface that far away.
f32 DistanceFor( f32 pixels )
{
    const f32 pixelsAtOne = static_cast<f32>( cSize ) / ( 2.0f * tan( Camera().mFov * 0.5f ) );
    return 0.5f * pixelsAtOne / pixels + sqrt( 2.0f );
}
}

TEST_CASE( "What the camera does not see is not drawn, and it still casts its shadow into view" )
{
    RenderFixture f( cSize );
    const AssetId box = f.ImportModel( "models/box.glb", MakeBox( vec3( 1.0f ) ) );
    Scene scene;
    AddModel( scene, "ahead", box, { 0.0f, 0.5f, -5.0f } );
    AddModel( scene, "behind", box, { 0.0f, 0.5f, 20.0f } );
    AddSun( scene );
    AddCamera( scene, Transform{ .mPosition = { 0.0f, 1.0f, 0.0f } } );
    World world( f.mApp, std::move( scene ) );
    f.Draw( world );

    const RenderStats& stats = f.mView.Stats();
    CHECK( stats.mItems == 2 );
    CHECK( stats.mVisible == 1 );
    CHECK( stats.mDrawCalls == 1 );
    CHECK( stats.mInstances == 1 );
    CHECK( stats.mTriangles == 12 );
    // Both in the light's box: one call for the two.
    CHECK( stats.mShadowDrawCalls == 1 );
}

TEST_CASE( "Meshes alike are drawn in one call, each where its own item is" )
{
    RenderFixture f( cSize );
    const AssetId box = f.ImportModel( "models/box.glb", MakeBox( vec3( 1.0f ) ) );
    const AssetId red = f.ImportMaterial( "materials/red.mat", R"({ "params": { "base_color": [ 1, 0, 0, 1 ] } })" );
    Scene scene;
    AddModel( scene, "left", box, { -2.0f, 0.5f, 0.0f } );
    AddModel( scene, "right", box, { 2.0f, 0.5f, 0.0f } );
    AddModel( scene, "red", box, { 0.0f, 0.5f, 2.5f }, { red } );
    AddSun( scene );
    AddCamera( scene,
               Transform{ .mPosition = { 0.0f, 12.0f, 0.0f }, .mRotation = { glm::radians( -90.0f ), 0.0f, 0.0f } } );
    World world( f.mApp, std::move( scene ) );
    const vector<u8> pixels = f.Draw( world );

    const RenderStats& stats = f.mView.Stats();
    CHECK( stats.mDrawCalls == 2 );
    CHECK( stats.mInstances == 3 );
    // Each instance drawn where its item is: both grey boxes, the sky
    // between them.
    const f32 reach = 11.0f * tan( Camera().mFov * 0.5f );
    const i32 left = static_cast<i32>( ( -2.0f / reach + 1.0f ) * 0.5f * cSize );
    const i32 right = static_cast<i32>( ( 2.0f / reach + 1.0f ) * 0.5f * cSize );
    const vec3 sky = ColorAt( pixels, cSize, cSize / 2, cSize / 2 );
    CHECK( sky.b > sky.r + 0.1f );
    for ( const i32 x : { left, right } )
    {
        CAPTURE( x );
        const vec3 top = ColorAt( pixels, cSize, x, cSize / 2 );
        CHECK( abs( top.r - top.b ) < 0.1f );
    }
}

TEST_CASE( "A mesh far away draws a coarser level, and one near draws the full mesh" )
{
    RenderFixture f( cSize );
    const AssetId square = f.ImportModel( "models/square.glb", Square() );
    for ( const auto& [distance, triangles] :
          { pair<f32, u64>{ DistanceFor( 8.0f ), 2 }, pair<f32, u64>{ DistanceFor( 0.3f ), 1 } } )
    {
        CAPTURE( distance );
        Scene scene;
        AddModel( scene, "square", square, { 0.0f, 0.0f, -distance } );
        AddCamera( scene, Transform{} );
        World world( f.mApp, std::move( scene ) );
        f.Draw( world );
        CHECK( f.mView.Stats().mTriangles == triangles );
    }
}

TEST_CASE( "A mesh at the edge between two levels keeps the one it had" )
{
    // Its error 0.9 pixels: within 1 to stay coarse, not within 0.75 to
    // grow coarse.
    const f32 edge = DistanceFor( 0.9f );
    for ( const auto& [from, triangles] :
          { pair<f32, u64>{ DistanceFor( 8.0f ), 2 }, pair<f32, u64>{ DistanceFor( 0.3f ), 1 } } )
    {
        CAPTURE( from );
        RenderFixture f( cSize );
        const AssetId square = f.ImportModel( "models/square.glb", Square() );
        Scene scene;
        const Entity entity = AddModel( scene, "square", square, { 0.0f, 0.0f, -from } );
        AddCamera( scene, Transform{} );
        World world( f.mApp, std::move( scene ) );
        f.Draw( world );
        world.GetScene().Get<Transform>( entity ).mPosition.z = -edge;
        f.Draw( world, 2 );
        CHECK( f.mView.Stats().mTriangles == triangles );
    }
}
#endif
