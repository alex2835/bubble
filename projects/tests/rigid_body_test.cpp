// rigid_body as data: its fields describe the Bullet body, which follows them
// at once out of a physics world and waits for the engine inside one.
#include "test.hpp"
#include "engine/physics/physics_engine.hpp"
#include "engine/utils/timer.hpp"
#include "engine/reflection/reflection.hpp"
#include "engine/scene/components/rigid_body_component.hpp"
#include "engine/scene/components/character_controller_component.hpp"
#include "engine/scene/components/transform_component.hpp"
#include <nlohmann/json.hpp>
#include <sol/sol.hpp>

namespace
{
int ShapeType( RigidBodyComponent& c ) { return c.mRigidBody.getShape()->getShapeType(); }
}

TEST( RigidBody_FieldsMakeTheBody )
{
    Fixture f;
    RigidBodyComponent body;
    entt::meta_any meta = Meta( body );

    // Out of a world the body is made again at once.
    SetField( meta, "shape", json( "box" ) );
    SetField( meta, "half_extents", vec3( 2, 3, 4 ) );
    SetField( meta, "mass", json( 5 ) );
    SetField( meta, "friction", json( 0.25 ) );
    CHECK( ShapeType( body ) == BOX_SHAPE_PROXYTYPE and body.mRigidBody.GetMass() == 5.0f );
    CHECK( body.mRigidBody.GetFriction() == 0.25f and not body.mRebuild );

    // Kinematic is massless, whatever the mass said.
    SetField( meta, "kinematic", json( true ) );
    CHECK( body.mMass == 0.0f and body.mRigidBody.GetMass() == 0.0f and body.mRigidBody.IsKinematic() );

    // Saved as its fields; loaded back into the same body.
    const json j = ToJson( meta );
    CHECK( j.at( "shape" ) == "box" and j.at( "kinematic" ) == true and not j.contains( "rigid_body" ) );
    RigidBodyComponent loaded;
    entt::meta_any loadedMeta = Meta( loaded );
    FromJson( j, loadedMeta );
    CHECK( ShapeType( loaded ) == BOX_SHAPE_PROXYTYPE and loaded.mRigidBody.IsKinematic() and loaded.mRigidBody.GetFriction() == 0.25f );
}

TEST( RigidBody_InAWorldWaits )
{
    PhysicsEngine physics;
    RigidBodyComponent body;
    body.mMass = 1.0f;
    body.Apply();
    physics.Add( body.mRigidBody, Entity( 1 ) );
    const btRigidBody* inWorld = body.mRigidBody.getBody();

    entt::meta_any meta = Meta( body );
    // Friction lands at once; a new mass waits for Engine::RebuildChangedBodies,
    // and the body the world holds is left alone meanwhile.
    SetField( meta, "friction", json( 0.75 ) );
    CHECK( body.mRigidBody.GetFriction() == 0.75f and not body.mRebuild );
    SetField( meta, "mass", json( 3 ) );
    CHECK( body.mRebuild and body.mRigidBody.getBody() == inWorld and body.mRigidBody.GetMass() == 1.0f );
    physics.Remove( body.mRigidBody );
}

TEST( RigidBody_FromScripts )
{
    Fixture f;
    PhysicsEngine physics;
    f.project.mScriptingEngine.BindLoader( f.project.mLoader );
    f.project.mScriptingEngine.BindScene( f.scene, physics );

    sol::state& lua = *f.project.mScriptingEngine.mLua;
    lua.script( R"(
        crate = spawn{ name = "crate", pos = vec3( 0, 5, 0 ),
                       rigid_body = { shape = "box", half_extents = vec3( 0.5 ), mass = 2 } }
        local body = crate:get_rigid_body()
        shape = body.shape
        body.friction = 0.9
        body:apply_central_impulse( vec3( 0, 1, 0 ) )
        typo_ok, typo_err = pcall( function() spawn{ rigid_body = { box = vec3( 1 ) } } end )
    )" );
    const Entity crate = lua["crate"].get<Entity>();
    auto& body = f.scene.GetComponent<RigidBodyComponent>( crate );
    CHECK( lua["shape"].get<string>() == "box" );
    CHECK( ShapeType( body ) == BOX_SHAPE_PROXYTYPE and body.mRigidBody.GetMass() == 2.0f );
    CHECK( body.mRigidBody.GetFriction() == 0.9f );
    CHECK( body.mRigidBody.getBody()->isInWorld() );
    CHECK( not lua["typo_ok"].get<bool>() and lua["typo_err"].get<string>().contains( "rigid_body has no field 'box'. Its fields: shape" ) );
    physics.Remove( body.mRigidBody );
}

// character_controller the same way: a capsule size makes the controller
// again, the rest lands on it at once.
TEST( CharacterController_FieldsMakeTheController )
{
    PhysicsEngine physics;
    CharacterControllerComponent character;
    entt::meta_any meta = Meta( character );

    SetField( meta, "radius", json( 0.8 ) );
    SetField( meta, "gravity", vec3( 0, -9, 0 ) );
    CHECK( character.mController.GetRadius() == 0.8f and character.mController.GetGravity() == vec3( 0, -9, 0 ) );
    SetField( meta, "max_slope", json( 0.5 ) );
    CHECK( character.mController.GetMaxSlopeRadians() == 0.5f );

    // In a world: speeds still land at once, a size waits.
    physics.Add( character.mController, Entity( 1 ) );
    SetField( meta, "jump_speed", json( 12 ) );
    SetField( meta, "height", json( 3 ) );
    CHECK( character.mController.GetJumpSpeed() == 12.0f and character.mRebuild and character.mController.GetHeight() == 1.0f );
    physics.Remove( character.mController );
    character.Rebuild();
    CHECK( character.mController.GetHeight() == 3.0f and character.mController.GetJumpSpeed() == 12.0f );
}

TEST( CharacterController_FromScripts )
{
    Fixture f;
    PhysicsEngine physics;
    f.project.mScriptingEngine.BindLoader( f.project.mLoader );
    f.project.mScriptingEngine.BindScene( f.scene, physics );

    sol::state& lua = *f.project.mScriptingEngine.mLua;
    lua.script( R"(
        hero = spawn{ name = "hero", pos = vec3( 0, 2, 0 ), character_controller = { radius = 0.4, height = 1.6 } }
        local c = hero:get_character_controller()
        c.fall_speed = 40
        c.gravity = vec3( 0, -20, 0 )
        half = c.height * 0.5 + c.radius
        c:set_walk_velocity( vec3( 1, 0, 0 ) )
        c:jump()
    )" );
    auto& character = f.scene.GetComponent<CharacterControllerComponent>( lua["hero"].get<Entity>() );
    CHECK( character.mController.GetFallSpeed() == 40.0f and character.mController.GetGravity() == vec3( 0, -20, 0 ) );
    CHECK( std::abs( lua["half"].get<f32>() - 1.2f ) < 1e-5f );
    physics.Remove( character.mController );
}

// A script that sets a field every frame - gravity, rising and falling -
// leaves alone the jump under way: Bullet caps the climb at its jump speed,
// and setting gravity used to put jump_speed back there.
TEST( CharacterController_JumpKeepsItsSpeed )
{
    Fixture f;
    PhysicsEngine physics;
    f.project.mScriptingEngine.BindLoader( f.project.mLoader );
    f.project.mScriptingEngine.BindScene( f.scene, physics );

    sol::state& lua = *f.project.mScriptingEngine.mLua;
    lua.script( R"(
        hero = spawn{ name = "hero", character_controller = { jump_speed = 10 } }
        hero:get_character_controller():jump( vec3( 0, 30, 0 ) )
        function frame() hero:get_character_controller().gravity = vec3( 0, -20, 0 ) end
    )" );
    auto& character = f.scene.GetComponent<CharacterControllerComponent>( lua["hero"].get<Entity>() );
    f32 top = 0.0f;
    for ( int i = 0; i < 120; i++ )
    {
        lua["frame"]();
        physics.Update( DeltaTime( 1.0f / 60.0f ) );
        top = std::max( top, character.mController.GetPosition().y );
    }
    // 30^2 / (2 * 20) = 22.5 up; at 10 it would stop short of 2.5.
    CHECK( top > 20.0f );
    physics.Remove( character.mController );
}
