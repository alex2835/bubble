// Components that describe themselves are bound to Lua from the description:
// a property per field, snake_case names, values by value.
#include "test.hpp"
#include "engine/physics/physics_engine.hpp"
#include "engine/scene/components/light_component.hpp"
#include "engine/scene/components/transform_component.hpp"
#include <sol/sol.hpp>

TEST( ReflectedLua_Components )
{
    Fixture f;
    PhysicsEngine physics;
    f.project.mScriptingEngine.BindLoader( f.project.mLoader );
    f.project.mScriptingEngine.BindScene( f.scene, physics );
    const Entity lamp = f.Create( EntityKind::Light );

    sol::state& lua = *f.project.mScriptingEngine.mLua;
    lua.script( R"(
        local lamp = level:find( "Light" )
        local l = lamp:get_light()
        -- An enum by the name of its value, or through its table.
        l.type = "spot"
        spot = l.type
        l.type = light_type.point
        point = l.type
        -- A set goes through the description: derived state follows.
        linear_before = l.linear
        l.distance = 13
        linear_after = l.linear
        -- Read only fields read, and refuse a set; a bad value says why.
        position_read = l.position ~= nil
        linear_set = pcall( function() l.linear = 1 end )
        bad_ok, bad_err = pcall( function() l.type = "cube" end )
        wrong_ok, wrong_err = pcall( function() l.brightness = "bright" end )

        local t = lamp:get_transform()
        t.rotation = vec3( 0, 1, 0 )
        yaw = t.rotation.y
        t.position = vec3( 1, 2, 3 )
        -- Constructors are snake_case too.
        made_x = transform( vec3( 4, 5, 6 ) ).position.x
        local c = camera()
        c.fov = 1.2
        fov = c.fov
        tag_name = lamp:get_tag().name
    )" );

    CHECK( lua["spot"].get<string>() == "spot" and lua["point"].get<string>() == "point" );
    CHECK( f.scene.GetComponent<LightComponent>( lamp ).mType == LightType::Point );
    CHECK( lua["linear_before"].get<f32>() != lua["linear_after"].get<f32>() );
    CHECK( f.scene.GetComponent<LightComponent>( lamp ).mDistance == 13.0f );
    CHECK( lua["position_read"].get<bool>() and not lua["linear_set"].get<bool>() );
    CHECK( not lua["bad_ok"].get<bool>() and lua["bad_err"].get<string>().contains( "has no value 'cube'" ) );
    CHECK( not lua["wrong_ok"].get<bool>() and lua["wrong_err"].get<string>().contains( "takes a number, not a string" ) );
    CHECK( std::abs( lua["yaw"].get<f32>() - 1.0f ) < 1e-5f );
    CHECK( f.scene.GetComponent<TransformComponent>( lamp ).mPosition == vec3( 1, 2, 3 ) );
    CHECK( lua["made_x"].get<f32>() == 4.0f );
    CHECK( std::abs( lua["fov"].get<f32>() - 1.2f ) < 1e-6f );
    CHECK( lua["tag_name"].get<string>() == "Light" );
}
