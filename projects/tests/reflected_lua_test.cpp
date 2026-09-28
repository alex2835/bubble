// Components that describe themselves are bound to Lua from the description:
// a property per field, snake_case names, values by value.
#include "test.hpp"
#include "engine/physics/physics_engine.hpp"
#include "engine/scene/components/light_component.hpp"
#include "engine/scene/components/transform_component.hpp"
#include "engine/scene/components/audio_source_component.hpp"
#include "engine/scene/components/animator_component.hpp"
#include <nlohmann/json.hpp>
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

// audio_source and animator: settings as flat fields over what plays.
TEST( ReflectedLua_AudioAndAnimator )
{
    Fixture f;
    PhysicsEngine physics;
    f.project.mScriptingEngine.BindLoader( f.project.mLoader );
    f.project.mScriptingEngine.BindScene( f.scene, physics );
    const Entity e = f.Create( EntityKind::GameObject );
    f.scene.AddComponent<AudioSourceComponent>( e );
    f.scene.AddComponent<AnimatorComponent>( e );

    sol::state& lua = *f.project.mScriptingEngine.mLua;
    lua.script( R"(
        local e = level:find( "Game object" )
        local s = e:get_audio_source()
        s.volume = 0.5
        s.spatialized = false
        s.play_on_start = true
        volume = s.volume
        local a = e:get_animator()
        a.clip = "walk"
        a.speed = 2
        clip = a.clip
        a.root_joint = "hips"
        a.root_motion = true
    )" );
    const auto& source = f.scene.GetComponent<AudioSourceComponent>( e );
    CHECK( source.mParams.mVolume == 0.5f and not source.mParams.mSpatialized and source.mPlayOnStart );
    CHECK( lua["volume"].get<f32>() == 0.5f );
    const auto& animator = f.scene.GetComponent<AnimatorComponent>( e );
    CHECK( animator.mBase.mClip == "walk" and animator.mBase.mSpeed == 2.0f and lua["clip"].get<string>() == "walk" );
    CHECK( animator.mRootJoint == "hips" and animator.mBase.mRootMotion );

    // Both saved as those fields, and read back.
    const json saved = f.project.mLevel.ToJson( f.project );
    const json& pools = saved.at( "Scene" ).at( "Component pools" );
    const json& savedSource = pools.at( "audio_source" ).at( std::to_string( e ) );
    CHECK( savedSource.at( "volume" ) == 0.5f and savedSource.at( "spatialized" ) == false and savedSource.at( "sound" ).is_null() );
    const json& savedAnimator = pools.at( "animator" ).at( std::to_string( e ) );
    CHECK( savedAnimator.at( "clip" ) == "walk" and savedAnimator.at( "controller" ).is_null() and savedAnimator.at( "root_motion" ) == true );
    Level loaded;
    loaded.FromJson( saved, f.project );
    CHECK( loaded.mScene.GetComponent<AudioSourceComponent>( e ).mParams.mVolume == 0.5f );
    CHECK( loaded.mScene.GetComponent<AnimatorComponent>( e ).mBase.mClip == "walk" );
}
