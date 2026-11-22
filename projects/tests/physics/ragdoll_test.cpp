// A ragdoll: a skinned model's bones as bodies when it is on - falling from
// the pose they had, drawn as they lie, pushed -, the animator's again when
// it is off, and played on from a snapshot as it did.
#include "bubble/render/render_scene.hpp"
#include "physics/physics_fixture.hpp"

using namespace bubble;
using namespace bubble::test;

namespace
{
// A figure of sticks, standing, facing +Z where its toe points, its bones
// named as Mixamo names them: hips, a spine, a neck, a head, an arm out to
// the side with a forearm, a hand and a finger too short to be a bone of its
// own, a leg with a knee and a foot.
ModelAsset Figure()
{
    ModelAsset model;
    const auto joint = [&]( string_view name, vec3 at, i32 parent ) {
        const mat4 rest = glm::translate( mat4( 1.0f ), at );
        model.mJoints.push_back( ModelJoint{
            .mName = string( name ), .mRest = rest, .mInverseBind = glm::inverse( rest ), .mParent = parent } );
    };
    joint( "Hips", { 0.0f, 0.9f, 0.0f }, -1 );
    joint( "Spine", { 0.0f, 1.3f, 0.0f }, 0 );
    joint( "Neck", { 0.0f, 1.6f, 0.0f }, 1 );
    joint( "Head", { 0.0f, 1.8f, 0.0f }, 2 );
    joint( "HeadTop_End", { 0.0f, 2.0f, 0.0f }, 3 );
    joint( "LeftArm", { 0.3f, 1.55f, 0.0f }, 1 );
    joint( "LeftForeArm", { 0.6f, 1.55f, 0.0f }, 5 );
    joint( "LeftHand", { 0.85f, 1.55f, 0.0f }, 6 );
    joint( "LeftHandIndex", { 0.88f, 1.55f, 0.0f }, 7 );
    joint( "LeftUpLeg", { 0.12f, 0.85f, 0.0f }, 0 );
    joint( "LeftLeg", { 0.12f, 0.48f, 0.0f }, 9 );
    joint( "LeftFoot", { 0.12f, 0.08f, 0.0f }, 10 );
    joint( "LeftToeBase", { 0.12f, 0.02f, 0.15f }, 11 );
    model.mMin = vec3( -0.3f, 0.0f, -0.3f );
    model.mMax = vec3( 0.8f, 2.0f, 0.3f );
    return model;
}

Entity Figure( PhysicsFixture& f, Scene& scene, bool active )
{
    const AssetId figure = f.AddModel( "models/figure.glb", Figure() );
    const Entity entity = PhysicsFixture::Place( scene, "figure", Transform{} );
    scene.Add<Model>( entity, Model{ .mAsset = figure, .mMaterials = {}, .mTint = {} } );
    scene.Add<Ragdoll>( entity, Ragdoll{ .mActive = active } );
    return entity;
}

// Where a joint is drawn, in the world: the skin's matrix undone, after a
// frame that takes no time - the skin is posed in the frame's phases.
vec3 JointAt( World& world, Entity entity, size_t joint )
{
    world.Frame( 0.0 );
    const SkinPose& skin = world.GetScene().Get<SkinPose>( entity );
    const mat4 pose = skin.mMatrices[joint] * glm::inverse( Figure().mJoints[joint].mInverseBind );
    return vec3( ComputeWorldMatrix( world.GetScene(), entity ) * pose[3] );
}

void SetActive( PhysicsFixture& f, World& world, Entity entity, bool active )
{
    const ComponentTypes& types = f.mApp.GetComponentTypes();
    REQUIRE( types.TrySetField( world.GetScene(), entity, *types.TryFind( "ragdoll"_n ),
                                *FieldPath::TryParse( "active" ), MetaAny( active ) ) );
}
}

TEST_CASE( "A ragdoll off leaves the model be; on, its bones fall to the ground and the model is drawn so" )
{
    PhysicsFixture f;
    Scene scene;
    PhysicsFixture::Ground( scene );
    const Entity figure = Figure( f, scene, false );
    World world( f.mApp, std::move( scene ) );
    f.Steps( world, 10 );
    CHECK_FALSE( world.GetScene().Has<SkinPose>( figure ) );

    SetActive( f, world, figure, true );
    f.Steps( world, 1 );
    // Where it stood, the moment it comes on.
    CHECK( JointAt( world, figure, 3 ).y == doctest::Approx( 1.8f ).epsilon( 0.02 ) );
    f.Steps( world, 240 );
    // Fallen: the head down by the ground, the finger still on the hand.
    CHECK( JointAt( world, figure, 3 ).y < 0.5f );
    CHECK( glm::distance( JointAt( world, figure, 8 ), JointAt( world, figure, 7 ) ) ==
           doctest::Approx( 0.03f ).epsilon( 0.05 ) );
    // The entity stays where it was; its bones are bodies a ray meets.
    CHECK( PhysicsFixture::PositionOf( world, figure ) == vec3( 0.0f ) );
    const vec3 hips = JointAt( world, figure, 0 );
    const auto hitMaybe =
        world.Get<PhysicsWorld>().TryRaycast( hips + vec3( 0.0f, 3.0f, 0.0f ), hips - vec3( 0.0f, 1.0f, 0.0f ) );
    REQUIRE( hitMaybe );
    CHECK( hitMaybe->mEntity == figure );
}

TEST_CASE( "A ragdoll pushed flies off; turned off, its pose is its animator's again" )
{
    PhysicsFixture f;
    Scene scene;
    const Entity figure = Figure( f, scene, true );
    World world( f.mApp, std::move( scene ) );
    f.Steps( world, 1 );
    REQUIRE( world.Get<PhysicsWorld>().TryPushRagdoll( figure, vec3( 700.0f, 0.0f, 0.0f ) ) );
    f.Steps( world, 30 );
    CHECK( JointAt( world, figure, 0 ).x > 2.0f );

    SetActive( f, world, figure, false );
    f.Steps( world, 1 );
    CHECK_FALSE( world.GetScene().Has<SkinPose>( figure ) );
    CHECK_FALSE( world.Get<PhysicsWorld>().TryPushRagdoll( figure, vec3( 1.0f ) ) );
}

TEST_CASE( "A ragdoll with no skinned model says why" )
{
    PhysicsFixture f;
    Scene scene;
    const Entity loose = PhysicsFixture::Place( scene, "loose", Transform{} );
    scene.Add<Ragdoll>( loose, Ragdoll{ .mActive = true } );
    const LogWatch log;
    World world( f.mApp, std::move( scene ) );
    f.Steps( world, 1 );
    CHECK( log.Saw( LogLevel::Error, { "/loose: ragdoll: no model - it goes on a skinned model" } ) );
}

TEST_CASE( "A snapshot of a ragdoll falling plays on as the world did" )
{
    PhysicsFixture f;
    World world( f.mApp, [&] {
        Scene scene;
        PhysicsFixture::Ground( scene );
        Figure( f, scene, true );
        PhysicsFixture::Crate( scene, "crate", vec3( 0.2f, 3.0f, 0.0f ) );
        return scene;
    }() );
    const Entity figure = *TryResolve( world.GetScene(), EntityPath{ *Text::TryFrom( "/figure" ) } );
    const auto joints = [&]( World& in ) {
        vector<vec3> at;
        for ( size_t joint = 0; joint < Figure().mJoints.size(); ++joint )
            at.push_back( JointAt( in, figure, joint ) );
        return at;
    };
    f.Steps( world, 20 );
    const WorldSnapshot snapshot = world.Snapshot();
    f.Steps( world, 60 );
    const vector<vec3> expected = joints( world );
    world.Restore( snapshot );
    f.Steps( world, 60 );
    CHECK( joints( world ) == expected );
}

TEST_CASE( "A ragdoll's joints keep a human's limits: a knee bends back, an elbow forward, the head turns only so far" )
{
    PhysicsFixture f;
    Scene scene;
    PhysicsFixture::Ground( scene );
    const Entity wall = PhysicsFixture::Place( scene, "wall", Transform{ .mPosition = { 1.5f, 1.0f, 0.0f } } );
    scene.Add<BoxCollider>( wall, BoxCollider{ .mSize = { 0.4f, 2.0f, 4.0f } } );
    const Entity figure = Figure( f, scene, true );
    World world( f.mApp, std::move( scene ) );
    f.Steps( world, 1 );
    const ModelAsset model = Figure();
    // A joint's turn from its parent's, from how it stood as the ragdoll came on.
    const auto relative = [&]( size_t joint ) {
        world.Frame( 0.0 );
        const SkinPose& skin = world.GetScene().Get<SkinPose>( figure );
        const auto pose = [&]( size_t j ) {
            return glm::quat_cast( mat3( skin.mMatrices[j] * glm::inverse( model.mJoints[j].mInverseBind ) ) );
        };
        return glm::inverse( pose( static_cast<size_t>( model.mJoints[joint].mParent ) ) ) * pose( joint );
    };
    // Its whole turn, and its turn about `axis`, degrees.
    const auto whole = []( const quat& q ) {
        return glm::degrees( 2.0f * std::acos( std::min( 1.0f, std::abs( q.w ) ) ) );
    };
    const auto about = []( quat q, vec3 axis ) {
        if ( q.w < 0.0f )
            q = -q;
        return glm::degrees( 2.0f * std::atan2( glm::dot( vec3( q.x, q.y, q.z ), axis ), q.w ) );
    };
    constexpr size_t cNeck = 2, cHead = 3, cForeArm = 6, cLeg = 10;
    const quat neck = relative( cNeck ), head = relative( cHead ), elbow = relative( cForeArm ),
               knee = relative( cLeg );
    f32 neckMost = 0, headMost = 0;
    f32 kneeLeast = 0, kneeMost = 0, elbowLeast = 0, elbowMost = 0;
    // Shoved into the wall at a run.
    REQUIRE( world.Get<PhysicsWorld>().TryPushRagdoll( figure, vec3( 300.0f, 60.0f, 40.0f ) ) );
    for ( int frame = 0; frame < 240; ++frame )
    {
        f.Steps( world, 1 );
        neckMost = std::max( neckMost, whole( glm::inverse( neck ) * relative( cNeck ) ) );
        headMost = std::max( headMost, whole( glm::inverse( head ) * relative( cHead ) ) );
        // The knee's hinge is X - back from the toe's +Z -, the elbow's -Y -
        // forward from the arm out along +X.
        const f32 kneeBent = about( glm::inverse( knee ) * relative( cLeg ), vec3( 1.0f, 0.0f, 0.0f ) );
        const f32 elbowBent = about( glm::inverse( elbow ) * relative( cForeArm ), vec3( 0.0f, -1.0f, 0.0f ) );
        kneeLeast = std::min( kneeLeast, kneeBent );
        kneeMost = std::max( kneeMost, kneeBent );
        elbowLeast = std::min( elbowLeast, elbowBent );
        elbowMost = std::max( elbowMost, elbowBent );
    }
    // Swing and twist together, and the give of a hard landing.
    CHECK( neckMost < 50.0f );
    CHECK( headMost < 60.0f );
    // Bent, and only the one way: never past straight by more than a little.
    CHECK( kneeMost > 20.0f );
    CHECK( kneeLeast > -10.0f );
    CHECK( elbowMost > 20.0f );
    CHECK( elbowLeast > -10.0f );
}
