// Joints: a body held to another or to the world - welded, hinged, sliding,
// on a rope, swinging and twisting -, driven by a motor, broken by more than
// it bears, kept as it was when a body is made again or a snapshot restored.
#include "bubble/types/algorithm.hpp"
#include "physics/physics_fixture.hpp"

using namespace bubble;
using namespace bubble::test;

namespace
{
EntityPath PathTo( string_view path )
{
    return EntityPath{ *Text::TryFrom( path ) };
}

// A door a unit wide and two high, its left edge on a hinge to the world
// along Y at x = 0.
Entity Door( Scene& scene, Joint joint = Joint{} )
{
    const Entity door = PhysicsFixture::Place( scene, "door", Transform{ .mPosition = { 0.5f, 1.0f, 0.0f } } );
    scene.Add<BoxCollider>( door, BoxCollider{ .mSize = { 1.0f, 2.0f, 0.1f } } );
    scene.Add<RigidBody>( door );
    joint.mKind = "hinge"_n;
    joint.mAnchor = vec3( -0.5f, 0.0f, 0.0f );
    joint.mAxis = vec3( 0.0f, 1.0f, 0.0f );
    scene.Add<Joint>( door, joint );
    return door;
}

vec3 HingePoint( World& world, Entity door )
{
    return vec3( ComputeWorldMatrix( world.GetScene(), door ) * vec4( -0.5f, 0.0f, 0.0f, 1.0f ) );
}

void SetField( PhysicsFixture& f, World& world, Entity entity, Name component, string_view field, MetaAny value )
{
    const ComponentTypes& types = f.mApp.GetComponentTypes();
    REQUIRE( types.TrySetField( world.GetScene(), entity, *types.TryFind( component ), *FieldPath::TryParse( field ),
                                std::move( value ) ) );
}
}

TEST_CASE( "A hinged door turns about its hinge when pushed, its hinge staying put, up to its limit" )
{
    PhysicsFixture f;
    Scene scene;
    const Entity door = Door( scene, Joint{ .mMin = -1.0f, .mMax = 1.0f } );
    World world( f.mApp, std::move( scene ) );
    f.Steps( world, 1 );
    PhysicsWorld& physics = world.Get<PhysicsWorld>();
    // Gravity pulls down the hinge's line: it holds the door up.
    f.Steps( world, 30 );
    CHECK( HingePoint( world, door ).y == doctest::Approx( 1.0f ).epsilon( 0.01 ) );

    REQUIRE( physics.TryAddImpulse( door, vec3( 0.0f, 0.0f, -3.0f ) ) );
    f.Steps( world, 60 );
    const vec3 hinge = HingePoint( world, door );
    CHECK( glm::length( vec2( hinge.x, hinge.z ) ) < 0.01f );
    const f32 turned = world.GetScene().Get<Transform>( door ).mRotation.y;
    // Pushed along -Z at its middle, it swings toward +Y's turn, and no
    // further than its limit.
    CHECK( turned > 0.5f );
    CHECK( turned < 1.02f );
}

TEST_CASE( "A ball on a rope swings below its hook, as far from it as it started" )
{
    PhysicsFixture f;
    Scene scene;
    const Entity hook = PhysicsFixture::Place( scene, "hook", Transform{ .mPosition = { 0.0f, 4.0f, 0.0f } } );
    const Entity ball = PhysicsFixture::Place( scene, "ball", Transform{ .mPosition = { 2.0f, 4.0f, 0.0f } } );
    scene.Add<SphereCollider>( ball, SphereCollider{ .mRadius = 0.2f } );
    scene.Add<RigidBody>( ball );
    scene.Add<Joint>( ball, Joint{ .mKind = "distance"_n, .mConnectedAnchor = { 0.0f, 4.0f, 0.0f } } );
    World world( f.mApp, std::move( scene ) );

    f.Steps( world, 20 );
    const vec3 swinging = PhysicsFixture::PositionOf( world, ball );
    CHECK( swinging.y < 3.5f );
    CHECK( glm::distance( swinging, PhysicsFixture::PositionOf( world, hook ) ) ==
           doctest::Approx( 2.0f ).epsilon( 0.01 ) );
    f.Steps( world, 200 );
    CHECK( glm::distance( PhysicsFixture::PositionOf( world, ball ), vec3( 0.0f, 4.0f, 0.0f ) ) ==
           doctest::Approx( 2.0f ).epsilon( 0.01 ) );
}

TEST_CASE( "Two bodies welded fall and tumble as one, and a joint holds a part's body by the part" )
{
    PhysicsFixture f;
    Scene scene;
    PhysicsFixture::Ground( scene );
    const Entity left = PhysicsFixture::Crate( scene, "left", vec3( 0.0f, 3.0f, 0.0f ) );
    scene.Get<Transform>( left ).mRotation = vec3( 0.3f, 0.2f, 0.5f );
    // The right crate is a body of one part: its joint is on the part.
    const Entity right = PhysicsFixture::Place( scene, "right", Transform{ .mPosition = { 1.0f, 3.0f, 0.0f } } );
    scene.Add<RigidBody>( right );
    const Entity part = PhysicsFixture::Place( scene, "part", Transform{}, right );
    scene.Add<BoxCollider>( part );
    scene.Add<Joint>( part, Joint{ .mKind = "fixed"_n, .mConnected = PathTo( "/left" ) } );
    World world( f.mApp, std::move( scene ) );
    // Welded where they stood: they are as far apart as they started.
    f.Steps( world, 240 );
    CHECK( glm::distance( PhysicsFixture::PositionOf( world, left ), PhysicsFixture::PositionOf( world, right ) ) ==
           doctest::Approx( 1.0f ).epsilon( 0.02 ) );
    CHECK( PhysicsFixture::PositionOf( world, left ).y < 1.6f );
}

TEST_CASE( "A slider's motor drives it at a velocity, or to a position from its rest, within its limits" )
{
    PhysicsFixture f;
    Scene scene;
    const Entity lift = PhysicsFixture::Place( scene, "lift", Transform{ .mPosition = { 0.0f, 1.0f, 0.0f } } );
    scene.Add<BoxCollider>( lift, BoxCollider{ .mSize = { 2.0f, 0.2f, 2.0f } } );
    scene.Add<RigidBody>( lift, RigidBody{ .mMass = 10, .mGravityScale = 0 } );
    scene.Add<Joint>( lift, Joint{ .mKind = "slider"_n,
                                   .mAxis = { 0.0f, 1.0f, 0.0f },
                                   .mMin = -0.5f,
                                   .mMax = 2.0f,
                                   .mMotor = "velocity"_n,
                                   .mTarget = 1.0f } );
    World world( f.mApp, std::move( scene ) );
    f.Steps( world, 60 );
    CHECK( PhysicsFixture::PositionOf( world, lift ).y == doctest::Approx( 2.0f ).epsilon( 0.03 ) );
    CHECK( PhysicsFixture::PositionOf( world, lift ).x == doctest::Approx( 0.0f ) );
    f.Steps( world, 120 );
    // Stopped at its upper limit.
    CHECK( PhysicsFixture::PositionOf( world, lift ).y == doctest::Approx( 3.0f ).epsilon( 0.02 ) );

    SetField( f, world, lift, "joint"_n, "motor", MetaAny( "position"_n ) );
    SetField( f, world, lift, "joint"_n, "target", MetaAny( 0.5f ) );
    f.Steps( world, 240 );
    CHECK( PhysicsFixture::PositionOf( world, lift ).y == doctest::Approx( 1.5f ).epsilon( 0.02 ) );
}

TEST_CASE( "A joint that bears more than its break force gives way, and its script hears" )
{
    PhysicsFixture f;
    const AssetId watcher = f.AddScript( "scripts/watcher.luau", R"(
        function on_joint_break( self: { entity: entity } )
            record( "broke " .. self.entity.name .. " " .. tostring( self.entity.joint.broken ) )
        end
    )" );
    Scene scene;
    const auto hang = [&]( string_view name, f32 x, f32 breakForce ) {
        const Entity crate = PhysicsFixture::Place( scene, name, Transform{ .mPosition = { x, 3.0f, 0.0f } } );
        scene.Add<BoxCollider>( crate );
        scene.Add<RigidBody>( crate );
        scene.Add<Joint>( crate, Joint{ .mKind = "fixed"_n, .mBreakForce = breakForce } );
        scene.Add<Script>( crate, Script{ .mAsset = watcher, .mProps = Value( Value::Record{} ) } );
        return crate;
    };
    // A unit mass weighs 9.81 newtons.
    const Entity weak = hang( "weak", 0.0f, 5.0f );
    const Entity strong = hang( "strong", 3.0f, 50.0f );
    World world( f.mApp, std::move( scene ) );
    f.Steps( world, 60 );

    CHECK( PhysicsFixture::PositionOf( world, weak ).y < 2.0f );
    CHECK( PhysicsFixture::PositionOf( world, strong ).y == doctest::Approx( 3.0f ).epsilon( 0.01 ) );
    CHECK( f.TakeRecorded() == vector<string>{ "broke weak true" } );
    CHECK( *world.Get<PhysicsWorld>().TryJointBroken( weak ) );
    CHECK_FALSE( *world.Get<PhysicsWorld>().TryJointBroken( strong ) );
}

TEST_CASE( "A body made again keeps its joint's rest: a door turned stays turned when its size changes" )
{
    PhysicsFixture f;
    Scene scene;
    const Entity door = Door( scene );
    World world( f.mApp, std::move( scene ) );
    f.Steps( world, 1 );
    REQUIRE( world.Get<PhysicsWorld>().TryAddImpulse( door, vec3( 0.0f, 0.0f, -2.0f ) ) );
    f.Steps( world, 30 );
    const f32 turned = world.GetScene().Get<Transform>( door ).mRotation.y;
    REQUIRE( turned > 0.3f );

    SetField( f, world, door, "box_collider"_n, "size", MetaAny( vec3( 1.0f, 2.0f, 0.3f ) ) );
    f.Steps( world, 1 );
    // Still on its hinge where the hinge was, not pulled back to its rest.
    const vec3 hinge = HingePoint( world, door );
    CHECK( glm::length( vec2( hinge.x, hinge.z ) ) < 0.01f );
    CHECK( world.GetScene().Get<Transform>( door ).mRotation.y > turned - 0.1f );
}

TEST_CASE( "A joint with nothing to hold, or holding what is not there, says why" )
{
    PhysicsFixture f;
    Scene scene;
    const Entity loose = PhysicsFixture::Place( scene, "loose", Transform{} );
    scene.Add<Joint>( loose );
    const Entity crate = PhysicsFixture::Crate( scene, "crate", vec3( 3.0f, 2.0f, 0.0f ) );
    scene.Add<Joint>( crate, Joint{ .mConnected = PathTo( "../nowhere" ) } );
    const LogWatch log;
    World world( f.mApp, std::move( scene ) );
    f.Steps( world, 2 );
    CHECK( log.Saw( LogLevel::Error, { "/loose: joint: no body to hold - a collider is missing" } ) );
    CHECK( log.Saw( LogLevel::Error, { "/crate: joint: connected: ", "nowhere" } ) );
}

TEST_CASE( "A snapshot of jointed bodies plays on as the world did, here and in another world" )
{
    PhysicsFixture f;
    const auto level = [] {
        Scene scene;
        PhysicsFixture::Ground( scene );
        // A chain of three links from a hook, swinging, and a door.
        Entity above = Entity::Null;
        for ( int i = 0; i < 3; ++i )
        {
            const Entity link =
                PhysicsFixture::Place( scene, format( "link_{}", i ),
                                       Transform{ .mPosition = { 0.6f * static_cast<f32>( i + 1 ), 5.0f, 0.0f } } );
            scene.Add<CapsuleCollider>( link, CapsuleCollider{ .mRadius = 0.1f, .mHeight = 0.6f } );
            scene.Get<Transform>( link ).mRotation = vec3( 0.0f, 0.0f, 1.5708f );
            scene.Add<RigidBody>( link );
            Joint joint{ .mKind = "swing_twist"_n, .mAnchor = { 0.0f, 0.3f, 0.0f }, .mAxis = { 0.0f, 1.0f, 0.0f } };
            if ( above != Entity::Null )
                joint.mConnected = PathTo( format( "../link_{}", i - 1 ) );
            scene.Add<Joint>( link, joint );
            above = link;
        }
        Door( scene, Joint{ .mMotor = "velocity"_n, .mTarget = 1.0f, .mMaxForce = 5.0f } );
        return scene;
    };
    const auto positions = []( World& world ) {
        vector<vec3> at;
        for ( const Entity entity : world.GetScene().EntitiesWith<Transform>() )
            at.push_back( world.GetScene().Get<Transform>( entity ).mPosition );
        return at;
    };
    World world( f.mApp, level() );
    f.Steps( world, 30 );
    const WorldSnapshot snapshot = world.Snapshot();
    f.Steps( world, 60 );
    const vector<vec3> expected = positions( world );

    world.Restore( snapshot );
    f.Steps( world, 60 );
    CHECK( positions( world ) == expected );

    World other( f.mApp, Scene() );
    other.Restore( snapshot );
    f.Steps( other, 60 );
    CHECK( positions( other ) == expected );
}
