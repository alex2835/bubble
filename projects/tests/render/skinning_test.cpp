// A skinned model drawn: at rest with no pose, bent by the pose the
// animation module writes - surfaces, shadow and picking each through their
// skinned pipelines. Not on the web, as gpu_test.cpp.
#ifndef __EMSCRIPTEN__
#include "render/render_fixture.hpp"
#include <doctest.h>

using namespace bubble;
using namespace bubble::test;

namespace
{
constexpr i32 cSize = 128;

// A strip 0.6 wide and 2 high on three joints - a root, "lower" at the
// origin, "upper" one up -: its bottom row on lower, its middle half on
// each, its top on upper.
ModelAsset Strip()
{
    ModelMesh mesh{ .mMaterial = "skin", .mVertices = {}, .mLods = {} };
    for ( const f32 y : { 0.0f, 1.0f, 2.0f } )
        for ( const f32 x : { -0.3f, 0.3f } )
        {
            mesh.mVertices.push_back(
                ModelVertex{ .mPosition = vec3( x, y, 0.0f ), .mNormal = vec3( 0, 0, 1 ), .mUv = vec2( 0.0f ) } );
            const f32 onUpper = y / 2.0f;
            mesh.mSkin.push_back(
                ModelSkinVertex{ .mJoints = { 1, 2, 0, 0 }, .mWeights = vec4( 1.0f - onUpper, onUpper, 0.0f, 0.0f ) } );
        }
    mesh.mLods.push_back( ModelLod{ .mError = 0, .mIndices = { 0, 1, 3, 0, 3, 2, 2, 3, 5, 2, 5, 4 } } );
    ModelAsset model{ .mMin = vec3( -0.3f, 0.0f, 0.0f ), .mMax = vec3( 0.3f, 2.0f, 0.0f ), .mMeshes = { mesh } };
    model.mJoints = { ModelJoint{ .mName = "root" }, ModelJoint{ .mName = "lower" },
                      ModelJoint{ .mName = "upper",
                                  .mRest = glm::translate( mat4( 1.0f ), vec3( 0.0f, 1.0f, 0.0f ) ),
                                  .mInverseBind = glm::translate( mat4( 1.0f ), vec3( 0.0f, -1.0f, 0.0f ) ) } };
    return model;
}

bool Covered( const vector<u8>& pixels, i32 x, i32 y )
{
    return glm::length( ColorAt( pixels, cSize, x, y ) - ColorAt( pixels, cSize, 0, 0 ) ) > 0.1f;
}
}

TEST_CASE( "A skinned model is drawn at rest with no pose, and bent as its pose has it" )
{
    RenderFixture f( cSize );
    Scene scene;
    const Entity strip = CreateChild( scene, scene.Root(), Make( "strip" ) );
    scene.Add<Transform>( strip );
    scene.Add<Model>( strip,
                      Model{ .mAsset = f.ImportModel( "models/strip.glb", Strip() ), .mMaterials = {}, .mTint = {} } );
    const Entity camera = CreateChild( scene, scene.Root(), Make( "camera" ) );
    scene.Add<Transform>( camera, Transform{ .mPosition = { 0.0f, 1.0f, 4.0f } } );
    scene.Add<Camera>( camera );
    const Entity sun = CreateChild( scene, scene.Root(), Make( "sun" ) );
    scene.Add<Transform>( sun, Transform{ .mRotation = { glm::radians( -20.0f ), glm::radians( 10.0f ), 0.0f } } );
    scene.Add<DirectionalLight>( sun );
    World world( f.mApp, std::move( scene ) );

    // Seen from 4 away: the strip's top half at (64, 42), and to its left,
    // where a bent top would go, (47, 60).
    const vector<u8> rest = f.Draw( world );
    CHECK( Covered( rest, 64, 42 ) );
    CHECK_FALSE( Covered( rest, 47, 60 ) );

    // Upper turned a quarter about Z: the top half points left.
    const mat4 bend = glm::translate( mat4( 1.0f ), vec3( 0.0f, 1.0f, 0.0f ) ) *
                      glm::mat4_cast( glm::angleAxis( glm::radians( 90.0f ), vec3( 0, 0, 1 ) ) ) *
                      glm::translate( mat4( 1.0f ), vec3( 0.0f, -1.0f, 0.0f ) );
    world.GetScene().Add<SkinPose>( strip, SkinPose{ .mMatrices = { mat4( 1.0f ), mat4( 1.0f ), bend } } );
    const vector<u8> bent = f.Draw( world, 2 );
    CHECK_FALSE( Covered( bent, 64, 42 ) );
    CHECK( Covered( bent, 47, 60 ) );

    // A click on the bent part finds the strip.
    f.mView.Pick( ivec2( 47, 60 ) );
    f.Draw( world, 1 );
    opt<Entity> picked;
    for ( u32 poll = 0; poll < 200 and not picked; ++poll )
    {
        f.mGpu->Poll();
        picked = f.mView.TryPicked();
    }
    CHECK( picked == strip );

    // A pose for another skeleton - of another length - is not this one's:
    // at rest.
    world.GetScene().Get<SkinPose>( strip ).mMatrices.pop_back();
    const vector<u8> again = f.Draw( world, 2 );
    CHECK( Covered( again, 64, 42 ) );
    CHECK_FALSE( Covered( again, 47, 60 ) );
}
#endif
