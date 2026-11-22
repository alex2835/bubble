// A body of several shapes: the colliders under a rigid_body, each where its
// transform puts it, and a convex hull of a model, which moves where a mesh
// cannot.
#include "physics/physics_fixture.hpp"

using namespace bubble;
using namespace bubble::test;

namespace
{
// A rigid_body with no shape of its own over two balls a unit either side.
Entity Dumbbell( Scene& scene, vec3 position )
{
    const Entity bar = PhysicsFixture::Place( scene, "dumbbell", Transform{ .mPosition = position } );
    scene.Add<RigidBody>( bar );
    for ( const f32 side : { -1.0f, 1.0f } )
    {
        const Entity ball = PhysicsFixture::Place( scene, side < 0.0f ? "left" : "right",
                                                   Transform{ .mPosition = { side, 0.0f, 0.0f } }, bar );
        scene.Add<SphereCollider>( ball );
    }
    return bar;
}

Entity Child( World& world, string_view path )
{
    return *TryResolve( world.GetScene(), EntityPath{ *Text::TryFrom( path ) } );
}
}

TEST_CASE( "The colliders under a rigid_body are one body, resting on them all, its parts no bodies of their own" )
{
    PhysicsFixture f;
    Scene scene;
    PhysicsFixture::Ground( scene );
    const Entity bar = Dumbbell( scene, vec3( 0.0f, 3.0f, 0.0f ) );
    World world( f.mApp, std::move( scene ) );
    f.Steps( world, 180 );

    const Transform& resting = world.GetScene().Get<Transform>( bar );
    CHECK( resting.mPosition.y == doctest::Approx( 0.5f ).epsilon( 0.02 ) );
    // Level on both balls, not rolled off one.
    CHECK( glm::abs( resting.mRotation.z ) < 0.01f );
    PhysicsWorld& physics = world.Get<PhysicsWorld>();
    CHECK( physics.TryVelocity( bar ) );
    CHECK_FALSE( physics.TryVelocity( Child( world, "/dumbbell/left" ) ) );
    // A ray at a ball hits the body it is part of.
    const auto hitMaybe = physics.TryRaycast( vec3( 1.0f, 5.0f, 0.0f ), vec3( 1.0f, -5.0f, 0.0f ) );
    REQUIRE( hitMaybe );
    CHECK( hitMaybe->mEntity == bar );
    CHECK( hitMaybe->mPoint.y == doctest::Approx( 1.0f ).epsilon( 0.02 ) );
}

TEST_CASE( "A body's weight is where its parts are: one off to the side falls straight, not about the origin" )
{
    PhysicsFixture f;
    Scene scene;
    PhysicsFixture::Ground( scene );
    const Entity body = PhysicsFixture::Place( scene, "body", Transform{ .mPosition = { 0.0f, 2.0f, 0.0f } } );
    scene.Add<RigidBody>( body );
    const Entity block = PhysicsFixture::Place( scene, "block", Transform{ .mPosition = { 3.0f, 0.0f, 0.0f } }, body );
    scene.Add<BoxCollider>( block );
    World world( f.mApp, std::move( scene ) );
    f.Steps( world, 180 );

    const Transform& resting = world.GetScene().Get<Transform>( body );
    CHECK( resting.mPosition.x == doctest::Approx( 0.0f ).epsilon( 0.02 ) );
    CHECK( resting.mPosition.y == doctest::Approx( 0.5f ).epsilon( 0.02 ) );
    CHECK( glm::length( resting.mRotation ) < 0.01f );
}

TEST_CASE( "A part changed or moved makes its body again; a rigid_body taken away leaves each collider its own" )
{
    PhysicsFixture f;
    Scene scene;
    PhysicsFixture::Ground( scene );
    const Entity bar = Dumbbell( scene, vec3( 0.0f, 0.5f, 0.0f ) );
    World world( f.mApp, std::move( scene ) );
    f.Steps( world, 60 );
    const ComponentTypes& types = f.mApp.GetComponentTypes();
    const Entity left = Child( world, "/dumbbell/left" );

    // The balls twice as large: it rests a unit high.
    for ( const Entity ball : { left, Child( world, "/dumbbell/right" ) } )
        REQUIRE( types.TrySetField( world.GetScene(), ball, *types.TryFind( "sphere_collider"_n ),
                                    *FieldPath::TryParse( "radius" ), MetaAny( 1.0f ) ) );
    f.Steps( world, 120 );
    CHECK( PhysicsFixture::PositionOf( world, bar ).y == doctest::Approx( 1.0f ).epsilon( 0.02 ) );

    // One ball lifted within the body: the body tips onto it no more - it
    // rests on the lower ball and the bar's end.
    REQUIRE( types.TrySetField( world.GetScene(), left, *types.TryFind( "transform"_n ),
                                *FieldPath::TryParse( "position" ), MetaAny( vec3( -1.0f, 4.0f, 0.0f ) ) ) );
    f.Steps( world, 1 );
    const auto hitMaybe = world.Get<PhysicsWorld>().TryRaycast( vec3( -1.0f, 9.0f, 0.0f ), vec3( -1.0f, 0.0f, 0.0f ) );
    REQUIRE( hitMaybe );
    CHECK( hitMaybe->mEntity == bar );
    CHECK( hitMaybe->mPoint.y > 5.0f );

    // No rigid_body: two balls standing where they are, each a body.
    world.GetScene().Remove<RigidBody>( bar );
    f.Steps( world, 1 );
    const auto standingMaybe =
        world.Get<PhysicsWorld>().TryRaycast( vec3( -1.0f, 9.0f, 0.0f ), vec3( -1.0f, 0.0f, 0.0f ) );
    REQUIRE( standingMaybe );
    CHECK( standingMaybe->mEntity == left );
}

TEST_CASE(
    "A convex collider wraps a model's points and falls with its rigid_body; a mesh in a moving body is left out" )
{
    PhysicsFixture f;
    const AssetId ball = f.AddModel( "models/ball.glb", MakeSphere( 0.5f ) );
    const AssetId plane = f.AddModel( "models/plane.glb", MakePlane( 2.0f ) );
    Scene scene;
    PhysicsFixture::Ground( scene );
    const Entity rock = PhysicsFixture::Place( scene, "rock", Transform{ .mPosition = { 0.0f, 3.0f, 0.0f } } );
    scene.Add<ConvexCollider>( rock, ConvexCollider{ .mModel = ball } );
    scene.Add<RigidBody>( rock );
    const Entity sheet = PhysicsFixture::Place( scene, "sheet", Transform{}, rock );
    scene.Add<MeshCollider>( sheet, MeshCollider{ .mModel = plane } );
    const LogWatch log;
    World world( f.mApp, std::move( scene ) );
    f.Steps( world, 180 );

    CHECK( PhysicsFixture::PositionOf( world, rock ).y == doctest::Approx( 0.5f ).epsilon( 0.03 ) );
    CHECK(
        log.Saw( LogLevel::Error,
                 { "/rock/sheet: a mesh_collider never moves: left out of /rock's body - a convex_collider moves" } ) );
}

TEST_CASE( "A snapshot of a body of parts plays on as the world did" )
{
    PhysicsFixture f;
    const auto level = [] {
        Scene scene;
        PhysicsFixture::Ground( scene );
        const Entity bar = Dumbbell( scene, vec3( 0.0f, 2.0f, 0.0f ) );
        scene.Get<Transform>( bar ).mRotation = vec3( 0.2f, 0.3f, 0.4f );
        PhysicsFixture::Crate( scene, "crate", vec3( 0.5f, 4.0f, 0.0f ) );
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
}
