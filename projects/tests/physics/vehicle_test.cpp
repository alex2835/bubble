// A vehicle: a body on wheels that rests on its suspension, drives where its
// throttle and wheel say, brakes, turns its wheels' children with the wheels,
// and plays on from a snapshot as it did.
#include "physics/physics_fixture.hpp"

using namespace bubble;
using namespace bubble::test;

namespace
{
// A floor 400 across whose top is at y = 0.
void Road( Scene& scene )
{
    const Entity road = PhysicsFixture::Place( scene, "road", Transform{ .mPosition = { 0.0f, -0.5f, 0.0f } } );
    scene.Add<BoxCollider>( road, BoxCollider{ .mSize = { 400.0f, 1.0f, 400.0f } } );
}

// A car of 1200 kilograms on four wheels, driven by the back two, steered by
// the front two; a hub under each wheel turns with it.
Entity Car( Scene& scene, vec3 position )
{
    const Entity car = PhysicsFixture::Place( scene, "car", Transform{ .mPosition = position } );
    scene.Add<BoxCollider>( car, BoxCollider{ .mSize = { 1.8f, 0.6f, 4.0f } } );
    scene.Add<RigidBody>( car, RigidBody{ .mMass = 1200, .mFriction = 0.5f } );
    scene.Add<Vehicle>( car, Vehicle{ .mMassOffset = { 0.0f, -0.4f, 0.0f } } );
    for ( const f32 z : { -1.4f, 1.4f } )
        for ( const f32 x : { -0.9f, 0.9f } )
        {
            const Entity wheel = PhysicsFixture::Place(
                scene, format( "wheel_{}_{}", z < 0.0f ? "front" : "back", x < 0.0f ? "left" : "right" ),
                Transform{ .mPosition = { x, -0.4f, z } }, car );
            const bool front = z < 0.0f;
            scene.Add<VehicleWheel>( wheel, VehicleWheel{ .mRadius = 0.4f,
                                                          .mMaxSteer = front ? 0.6f : 0.0f,
                                                          .mDrive = not front,
                                                          .mHandbrake = not front } );
            PhysicsFixture::Place( scene, "hub", Transform{}, wheel );
        }
    return car;
}

Vehicle& Driver( World& world, Entity car )
{
    return world.GetScene().Get<Vehicle>( car );
}
}

TEST_CASE( "A vehicle comes to rest on its wheels, the ground under them, and its gauges read nought" )
{
    PhysicsFixture f;
    Scene scene;
    Road( scene );
    const Entity car = Car( scene, vec3( 0.0f, 1.2f, 0.0f ) );
    World world( f.mApp, std::move( scene ) );
    f.Steps( world, 180 );

    const Transform& resting = world.GetScene().Get<Transform>( car );
    // On its suspension: the wheels' bottoms on the road, the body above it.
    CHECK( resting.mPosition.y > 0.5f );
    CHECK( resting.mPosition.y < 1.0f );
    CHECK( glm::length( resting.mRotation ) < 0.02f );
    const auto gaugesMaybe = world.Get<PhysicsWorld>().TryVehicle( car );
    REQUIRE( gaugesMaybe );
    CHECK( std::abs( gaugesMaybe->mSpeed ) < 0.05f );
}

TEST_CASE( "A vehicle drives forward on its throttle, turns with its wheel, and stops on its brake" )
{
    PhysicsFixture f;
    Scene scene;
    Road( scene );
    const Entity car = Car( scene, vec3( 0.0f, 1.0f, 0.0f ) );
    World world( f.mApp, std::move( scene ) );
    PhysicsWorld& physics = world.Get<PhysicsWorld>();
    f.Steps( world, 60 );

    Driver( world, car ).mThrottle = 1.0f;
    f.Steps( world, 180 );
    // Forward is -Z.
    const vec3 driven = PhysicsFixture::PositionOf( world, car );
    CHECK( driven.z < -5.0f );
    CHECK( std::abs( driven.x ) < 0.3f );
    const VehicleGauges gauges = *physics.TryVehicle( car );
    CHECK( gauges.mSpeed > 4.0f );
    CHECK( gauges.mRpm > 1000.0f );
    CHECK( gauges.mGear >= 1 );

    // Right, toward +X.
    Driver( world, car ).mSteer = 1.0f;
    f.Steps( world, 90 );
    CHECK( PhysicsFixture::PositionOf( world, car ).x > driven.x + 1.0f );

    Driver( world, car ) = Vehicle{ .mMassOffset = { 0.0f, -0.4f, 0.0f }, .mBrake = 1.0f };
    f.Steps( world, 240 );
    CHECK( std::abs( physics.TryVehicle( car )->mSpeed ) < 0.1f );
}

TEST_CASE( "A wheel's children turn with it as it rolls and stay where it is at rest" )
{
    PhysicsFixture f;
    Scene scene;
    Road( scene );
    const Entity car = Car( scene, vec3( 0.0f, 1.0f, 0.0f ) );
    World world( f.mApp, std::move( scene ) );
    f.Steps( world, 120 );
    const Entity hub = *TryResolve( world.GetScene(), EntityPath{ *Text::TryFrom( "/car/wheel_back_left/hub" ) } );
    const auto restingMaybe = world.GetScene().TryGet<ParentOffset>( hub );
    REQUIRE( restingMaybe );
    // Not turned at rest, and raised by its suspension no more than it goes.
    const mat4 resting = restingMaybe->mMatrix;
    CHECK( resting[0][0] == doctest::Approx( 1.0f ).epsilon( 0.01 ) );
    CHECK( resting[3].y >= -0.01f );
    CHECK( resting[3].y <= 0.31f );

    Driver( world, car ).mThrottle = 1.0f;
    f.Steps( world, 37 );
    // Rolling: turned about its axle, X.
    const mat4 rolling = world.GetScene().Get<ParentOffset>( hub ).mMatrix;
    CHECK( rolling[0][0] == doctest::Approx( 1.0f ).epsilon( 0.01 ) );
    CHECK( std::abs( rolling[1][1] - 1.0f ) > 0.01f );
}

TEST_CASE( "A vehicle with no wheels, or no body to drive, says why" )
{
    PhysicsFixture f;
    Scene scene;
    const Entity bare = PhysicsFixture::Crate( scene, "bare", vec3( 0.0f, 1.0f, 0.0f ) );
    scene.Add<Vehicle>( bare );
    const Entity ghost = PhysicsFixture::Place( scene, "ghost", Transform{} );
    scene.Add<Vehicle>( ghost );
    const LogWatch log;
    World world( f.mApp, std::move( scene ) );
    f.Steps( world, 2 );
    CHECK( log.Saw( LogLevel::Error, { "/bare: vehicle: no wheels - entities under it with a vehicle_wheel" } ) );
    CHECK( log.Saw( LogLevel::Error,
                    { "/ghost: vehicle: no body that moves - it goes on a rigid_body with colliders" } ) );
}

TEST_CASE( "A snapshot of a vehicle driving plays on as the world did" )
{
    PhysicsFixture f;
    const auto level = [] {
        Scene scene;
        Road( scene );
        const Entity car = Car( scene, vec3( 0.0f, 1.0f, 0.0f ) );
        scene.Get<Vehicle>( car ).mThrottle = 1.0f;
        scene.Get<Vehicle>( car ).mSteer = 0.4f;
        PhysicsFixture::Crate( scene, "crate", vec3( 3.0f, 0.5f, -6.0f ) );
        return scene;
    };
    const auto positions = []( World& world ) {
        vector<vec3> at;
        for ( const Entity entity : world.GetScene().EntitiesWith<Transform>() )
            at.push_back( world.GetScene().Get<Transform>( entity ).mPosition );
        return at;
    };
    World world( f.mApp, level() );
    f.Steps( world, 60 );
    const WorldSnapshot snapshot = world.Snapshot();
    f.Steps( world, 90 );
    const vector<vec3> expected = positions( world );
    world.Restore( snapshot );
    f.Steps( world, 90 );
    CHECK( positions( world ) == expected );
}
