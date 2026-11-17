// The physics module on Jolt: bodies made from colliders, simulated or
// moved by their transforms, characters that walk, rays and overlaps, what
// scripts reach, and a world's snapshot that plays on the same.
#include "bubble/scripts/script_declarations.hpp"
#include "bubble/types/algorithm.hpp"
#include "physics/physics_fixture.hpp"

using namespace bubble;
using namespace bubble::test;

TEST_CASE( "A box falls under gravity and comes to rest on the ground, its transform following" )
{
    PhysicsFixture f;
    Scene scene;
    PhysicsFixture::Ground( scene );
    const Entity crate = PhysicsFixture::Crate( scene, "crate", vec3( 0.0f, 5.0f, 0.0f ) );
    World world( f.mApp, std::move( scene ) );

    f.Steps( world, 30 );
    // Half a second of falling: about g t^2 / 2 = 1.2 down, less damping.
    const f32 fallen = 5.0f - PhysicsFixture::PositionOf( world, crate ).y;
    CHECK( fallen > 1.0f );
    CHECK( fallen < 1.3f );
    f.Steps( world, 150 );
    const Transform& resting = world.GetScene().Get<Transform>( crate );
    CHECK( resting.mPosition.y == doctest::Approx( 0.5f ).epsilon( 0.02 ) );
    CHECK( glm::length( resting.mRotation ) < 0.01f );
    CHECK( glm::length( *world.Get<PhysicsWorld>().TryVelocity( crate ) ) < 0.05f );
}

TEST_CASE( "A kinematic platform goes where its transform says and carries what stands on it" )
{
    PhysicsFixture f;
    Scene scene;
    PhysicsFixture::Ground( scene );
    const Entity platform = PhysicsFixture::Place( scene, "platform", Transform{ .mPosition = { 0.0f, 1.0f, 0.0f } } );
    scene.Add<BoxCollider>( platform, BoxCollider{ .mSize = { 4.0f, 0.5f, 4.0f } } );
    scene.Add<RigidBody>( platform, RigidBody{ .mMass = 1,
                                               .mFriction = 1,
                                               .mRestitution = 0,
                                               .mLinearDamping = 0,
                                               .mAngularDamping = 0,
                                               .mGravityScale = 1,
                                               .mKinematic = true } );
    const Entity crate = PhysicsFixture::Crate( scene, "crate", vec3( 0.0f, 1.8f, 0.0f ) );
    World world( f.mApp, std::move( scene ) );
    f.Steps( world, 60 );

    // A second sliding 1 a second along X: what moves it is its transform.
    for ( u32 step = 0; step < 60; ++step )
    {
        world.GetScene().Get<Transform>( platform ).mPosition.x += 1.0f / 60.0f;
        f.Steps( world, 1 );
    }
    CHECK( PhysicsFixture::PositionOf( world, platform ).x == doctest::Approx( 1.0f ) );
    const vec3 carried = PhysicsFixture::PositionOf( world, crate );
    CHECK( carried.x > 0.8f );
    CHECK( carried.y == doctest::Approx( 1.75f ).epsilon( 0.02 ) );
}

TEST_CASE( "A character walks where its velocity says, keeps to the ground and climbs a step" )
{
    PhysicsFixture f;
    Scene scene;
    PhysicsFixture::Ground( scene );
    const Entity step = PhysicsFixture::Place( scene, "step", Transform{ .mPosition = { 3.0f, 0.1f, 0.0f } } );
    scene.Add<BoxCollider>( step, BoxCollider{ .mSize = { 2.0f, 0.2f, 4.0f } } );
    const Entity hero = PhysicsFixture::Place( scene, "hero", Transform{ .mPosition = { 0.0f, 0.5f, 0.0f } } );
    scene.Add<CharacterController>( hero );
    World world( f.mApp, std::move( scene ) );
    PhysicsWorld& physics = world.Get<PhysicsWorld>();

    // Gravity is the script's, as a game's character script has it: none
    // while on the ground, falling faster while not.
    const auto walk = [&]( f32 speed, u32 steps ) {
        for ( u32 at = 0; at < steps; ++at )
        {
            const CharacterState state = *physics.TryCharacter( hero );
            const f32 fall = state.mOnGround ? 0.0f : state.mVelocity.y + physics.Gravity().y / 60.0f;
            REQUIRE( physics.TrySetCharacterVelocity( hero, vec3( speed, fall, 0.0f ) + state.mGroundVelocity ) );
            f.Steps( world, 1 );
        }
    };
    walk( 0.0f, 60 );
    CHECK( physics.TryCharacter( hero )->mOnGround );
    CHECK( PhysicsFixture::PositionOf( world, hero ).y == doctest::Approx( 0.0f ).epsilon( 0.02 ) );

    walk( 2.0f, 120 );
    const vec3 walked = PhysicsFixture::PositionOf( world, hero );
    CHECK( walked.x > 3.5f );
    // Up on the step.
    CHECK( walked.y == doctest::Approx( 0.2f ).epsilon( 0.03 ) );
    CHECK( physics.TryCharacter( hero )->mOnGround );
    CHECK_FALSE( physics.TryCharacter( step ) );
}

TEST_CASE( "A ray finds the first body it meets; an overlap, every body touching a sphere" )
{
    PhysicsFixture f;
    Scene scene;
    const Entity ground = PhysicsFixture::Ground( scene );
    const Entity near = PhysicsFixture::Place( scene, "near", Transform{ .mPosition = { 0.0f, 0.5f, 0.0f } } );
    scene.Add<BoxCollider>( near );
    const Entity far = PhysicsFixture::Place( scene, "far", Transform{ .mPosition = { 5.0f, 0.5f, 0.0f } } );
    scene.Add<SphereCollider>( far );
    World world( f.mApp, std::move( scene ) );
    PhysicsWorld& physics = world.Get<PhysicsWorld>();

    const auto hitMaybe = physics.TryRaycast( vec3( -5.0f, 0.5f, 0.0f ), vec3( 10.0f, 0.5f, 0.0f ) );
    REQUIRE( hitMaybe );
    CHECK( hitMaybe->mEntity == near );
    CHECK( hitMaybe->mPoint.x == doctest::Approx( -0.5f ) );
    CHECK( hitMaybe->mNormal.x == doctest::Approx( -1.0f ) );
    CHECK( hitMaybe->mDistance == doctest::Approx( 4.5f ) );
    const auto downMaybe = physics.TryRaycast( vec3( 3.0f, 5.0f, 0.0f ), vec3( 3.0f, -5.0f, 0.0f ) );
    REQUIRE( downMaybe );
    CHECK( downMaybe->mEntity == ground );
    CHECK_FALSE( physics.TryRaycast( vec3( 0.0f, 5.0f, 0.0f ), vec3( 0.0f, 10.0f, 0.0f ) ) );

    const vector<Entity> touching = physics.OverlapSphere( vec3( 5.0f, 1.0f, 0.8f ), 0.5f );
    CHECK( touching == vector<Entity>{ far } );
}

TEST_CASE( "Changing a collider remakes the body; a transform set moves it there; no rigid_body, it stands" )
{
    PhysicsFixture f;
    Scene scene;
    PhysicsFixture::Ground( scene );
    const Entity crate = PhysicsFixture::Crate( scene, "crate", vec3( 0.0f, 0.5f, 0.0f ) );
    World world( f.mApp, std::move( scene ) );
    f.Steps( world, 30 );
    const ComponentTypes& types = f.mApp.GetComponentTypes();

    // Twice as large: it rests twice as high.
    REQUIRE( types.TrySetField( world.GetScene(), crate, *types.TryFind( "box_collider"_n ),
                                *FieldPath::TryParse( "size" ), MetaAny( vec3( 2.0f ) ) ) );
    f.Steps( world, 90 );
    CHECK( PhysicsFixture::PositionOf( world, crate ).y == doctest::Approx( 1.0f ).epsilon( 0.02 ) );

    REQUIRE( types.TrySetField( world.GetScene(), crate, *types.TryFind( "transform"_n ),
                                *FieldPath::TryParse( "position" ), MetaAny( vec3( 4.0f, 3.0f, 0.0f ) ) ) );
    f.Steps( world, 1 );
    CHECK( PhysicsFixture::PositionOf( world, crate ).x == doctest::Approx( 4.0f ) );

    // Without its rigid_body it stays in the air, no longer a moving body.
    world.GetScene().Remove<RigidBody>( crate );
    f.Steps( world, 30 );
    CHECK( PhysicsFixture::PositionOf( world, crate ).y > 2.5f );
    CHECK_FALSE( world.Get<PhysicsWorld>().TryVelocity( crate ) );
}

TEST_CASE( "A body falls onto a model's triangles once the model is in" )
{
    PhysicsFixture f;
    const AssetId plane = f.AddModel( "models/plane.glb", MakePlane( 10.0f ) );
    Scene scene;
    const Entity floor = PhysicsFixture::Place( scene, "floor", Transform{} );
    scene.Add<MeshCollider>( floor, MeshCollider{ .mModel = plane } );
    const Entity crate = PhysicsFixture::Crate( scene, "crate", vec3( 1.0f, 2.0f, 1.0f ) );
    World world( f.mApp, std::move( scene ) );
    f.Steps( world, 120 );
    CHECK( PhysicsFixture::PositionOf( world, crate ).y == doctest::Approx( 0.5f ).epsilon( 0.02 ) );
}

TEST_CASE( "A collider that cannot be made, or two on one entity, is said in the log" )
{
    PhysicsFixture f;
    Scene scene;
    const Entity flat = PhysicsFixture::Place( scene, "flat", Transform{} );
    scene.Add<BoxCollider>( flat, BoxCollider{ .mSize = { 1.0f, 0.0f, 1.0f } } );
    const Entity both = PhysicsFixture::Place( scene, "both", Transform{ .mPosition = { 3.0f, 0.0f, 0.0f } } );
    scene.Add<BoxCollider>( both );
    scene.Add<SphereCollider>( both );
    const LogWatch log;
    World world( f.mApp, std::move( scene ) );
    f.Steps( world, 1 );
    CHECK( log.Saw( LogLevel::Error, { "/flat: box_collider.size: every side above 0" } ) );
    CHECK(
        log.Saw( LogLevel::Error,
                 { "/both: one collider an entity, it has box_collider, sphere_collider: the box_collider counts" } ) );
}

TEST_CASE( "Scripts read and set velocities, push bodies, ask rays and hear collisions" )
{
    PhysicsFixture f;
    const AssetId thrower = f.AddScript( "scripts/thrower.luau", R"(
function on_start( self )
    self.entity.rigid_body.velocity = vector.create( 3, 0, 0 )
    self.entity.rigid_body:apply_impulse( vector.create( 0, 2, 0 ) )
    local v = self.entity.rigid_body.velocity
    record( string.format( "%g %g", v.x, v.y ) )
    local hit = physics.raycast( vector.create( 0, 10, 0 ), vector.create( 0, -10, 0 ) )
    record( hit and hit.entity.name or "nothing" )
end

function on_collision( self, other, point, normal )
    record( "hit " .. other.name .. " " .. string.format( "%.0f", normal.y ) )
end
)" );
    const AssetId walker = f.AddScript( "scripts/walker.luau", R"(
function on_fixed_update( self, dt )
    local c = self.entity.character_controller
    c.velocity = vector.create( 0, -5, 0 )
    if c.on_ground and not self.landed then
        self.landed = true
        record( "landed" )
    end
end
locals { landed = false }
)" );
    Scene scene;
    PhysicsFixture::Ground( scene );
    const Entity crate = PhysicsFixture::Crate( scene, "crate", vec3( 0.0f, 2.0f, 0.0f ) );
    scene.Add<Script>( crate, Script{ .mAsset = thrower, .mProps = Value( Value::Record{} ) } );
    const Entity hero = PhysicsFixture::Place( scene, "hero", Transform{ .mPosition = { 5.0f, 1.0f, 0.0f } } );
    scene.Add<CharacterController>( hero );
    scene.Add<Script>( hero, Script{ .mAsset = walker, .mProps = Value( Value::Record{} ) } );
    World world( f.mApp, std::move( scene ) );
    f.Steps( world, 120 );

    const vector<string> recorded = f.TakeRecorded();
    REQUIRE( recorded.size() >= 4 );
    CHECK( recorded[0] == "3 2" );
    CHECK( recorded[1] == "crate" );
    CHECK( ranges::count( recorded, "hit ground 1"s ) >= 1 );
    CHECK( ranges::count( recorded, "landed"s ) == 1 );
}

TEST_CASE( "A snapshot plays on as the world it was taken from did, here and in another world" )
{
    PhysicsFixture f;
    const auto level = [] {
        Scene scene;
        PhysicsFixture::Ground( scene );
        for ( int i = 0; i < 6; ++i )
        {
            const Entity crate = PhysicsFixture::Crate(
                scene, "crate", vec3( 0.3f * static_cast<f32>( i % 2 ), 1.0f + 1.1f * static_cast<f32>( i ), 0.2f ) );
            scene.Get<Transform>( crate ).mRotation = vec3( 0.1f * static_cast<f32>( i ), 0.3f, 0.0f );
        }
        const Entity hero = PhysicsFixture::Place( scene, "hero", Transform{ .mPosition = { 2.0f, 1.0f, 0.0f } } );
        scene.Add<CharacterController>( hero );
        return scene;
    };
    const auto positions = []( World& world ) {
        vector<vec3> at;
        for ( const Entity entity : world.GetScene().EntitiesWith<Transform>() )
            at.push_back( world.GetScene().Get<Transform>( entity ).mPosition );
        return at;
    };

    World world( f.mApp, level() );
    const Entity hero = *TryResolve( world.GetScene(), EntityPath{ *Text::TryFrom( "/hero" ) } );
    world.Get<PhysicsWorld>().TrySetCharacterVelocity( hero, vec3( -1.0f, -3.0f, 0.0f ) );
    f.Steps( world, 40 );
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

TEST_CASE( "What physics gives scripts is declared, and a typed script using it passes the strict check" )
{
    PhysicsFixture f;
    const string declarations = WriteScriptDeclarations( f.mApp );
    CHECK( declarations.find( "    kinematic: boolean\n    velocity: vector\n    angular_velocity: vector\n" ) !=
           string::npos );
    CHECK( declarations.find( "    on_ground: boolean\n" ) != string::npos );
    CHECK( declarations.find( "declare physics: {\n" ) != string::npos );
    CHECK( declarations.find( "--     on_collision( self, other: entity, point: vector, normal: vector )\n" ) !=
           string::npos );

    hmap<string, string> files;
    const Importer importer =
        ScriptImporter( ScriptCheck{ .mDeclarations = declarations, .mAliases = {}, .mRead = {}, .mAssetOf = {} } );
    const string source = R"(
local P = props { jump = 5 }
type Self = typeof( P ) & { entity: entity }

function on_fixed_update( self: Self, dt: number )
    local body = self.entity.character_controller
    local fall = if body.on_ground then 0 else body.velocity.y + physics.gravity().y * dt
    body.velocity = vector.create( 0, fall, 0 ) + body.ground_velocity
    local hit = physics.raycast( vector.zero, vector.create( 0, -10, 0 ) )
    if hit then
        hit.entity.rigid_body:apply_impulse( hit.normal * self.jump )
    end
end

function on_collision( self: Self, other: entity, point: vector, normal: vector )
    other.rigid_body.velocity = normal
end
)";
    const auto importedMaybe = importer.mImport( source, Json(), *AssetPath::TryFrom( "scripts/hero.luau" ) );
    REQUIRE_MESSAGE( importedMaybe, importedMaybe.error() );
    string wrong = source;
    wrong.replace( wrong.find( "body.on_ground" ), 14, "body.on_grond" );
    CHECK_FALSE( importer.mImport( wrong, Json(), *AssetPath::TryFrom( "scripts/hero.luau" ) ) );
}

TEST_CASE( "A script spawns a hundred crates from a prefab, its own scripts start, and they fall and pile up" )
{
    PhysicsFixture f;
    const AssetId spinner = f.AddScript( "scripts/spinner.luau", "function on_start( self ) record( 'up' ) end" );
    f.Add( "prefabs/crate.prefab",
           format( R"({{ "version": 1, "next_id": 1, "root": {{ "id": 0, "components": {{
                 "transform": {{}}, "box_collider": {{}}, "rigid_body": {{}},
                 "script": {{ "asset": {{ "asset": "{}" }}, "props": {{}} }} }} }} }})",
                   spinner.ToString() ),
           cPrefabKind );
    const AssetId dropper = f.AddScript( "scripts/dropper.luau", R"(
local P = props { crate = asset( "prefabs/crate.prefab" ) }
function on_start( self )
    for i = 0, 99 do
        spawn( self.crate, nil, vector.create( ( i % 10 ) * 1.2 - 6, 1 + math.floor( i / 10 ) * 1.2, 0 ) )
    end
end
)" );
    Scene scene;
    PhysicsFixture::Ground( scene );
    const Entity hand = PhysicsFixture::Place( scene, "hand", Transform{} );
    scene.Add<Script>( hand, Script{ .mAsset = dropper, .mProps = Value( Value::Record{} ) } );
    World world( f.mApp, std::move( scene ) );
    f.Steps( world, 240 );

    const vector<Entity> crates = world.GetScene().EntitiesWith<RigidBody>();
    REQUIRE( crates.size() == 100 );
    // Named after the prefab's file, unique among their siblings.
    vector<string> names;
    for ( const Entity crate : crates )
        names.push_back( NameOf( world.GetScene(), crate ).String() );
    CHECK( ranges::count( names, "crate"s ) == 1 );
    CHECK( ranges::count( names, "crate_100"s ) == 1 );
    f32 highest = 0;
    for ( const Entity crate : crates )
    {
        CHECK( PhysicsFixture::PositionOf( world, crate ).y > 0.4f );
        highest = max( highest, PhysicsFixture::PositionOf( world, crate ).y );
    }
    // Stacked ten high at most, each on the one below.
    CHECK( highest < 12.0f );
    CHECK( ranges::count( f.TakeRecorded(), "up"s ) == 100 );
}

TEST_CASE( "Spawning what is not a prefab is the script's error, naming it" )
{
    PhysicsFixture f;
    const AssetId script = f.AddScript( "scripts/wrong.luau", R"(
local P = props { thing = asset( "scripts/wrong.luau" ) }
function on_start( self ) spawn( self.thing ) end
)" );
    Scene scene;
    const Entity hand = PhysicsFixture::Place( scene, "hand", Transform{} );
    scene.Add<Script>( hand, Script{ .mAsset = script, .mProps = Value( Value::Record{} ) } );
    const LogWatch log;
    World world( f.mApp, std::move( scene ) );
    f.Steps( world, 5 );
    CHECK( log.Saw( LogLevel::Error, { "/hand: scripts/wrong.luau:3: spawn( scripts/wrong.luau ):" } ) );
}
