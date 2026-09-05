#include "engine/pch/pch.hpp"
#include "engine/scripting/bindings/spawn_lua_bindings.hpp"
#include "engine/scene/scene.hpp"
#include "engine/loader/loader.hpp"
#include "engine/physics/physics_engine.hpp"
#include "engine/scripting/scripting_engine.hpp"
#include "binding_utils.hpp"
#include <sol/sol.hpp>

namespace bubble
{
// Optional field of a spawn table. Absent and wrong-typed are both "not given",
// because a spawn table is written by hand and a typo'd key should not be a
// silently ignored setting.
template <class T>
static std::optional<T> Field( const sol::table& table, const char* key )
{
    const sol::object value = table[key];
    if ( not value.valid() or value.is<sol::nil_t>() )
        return std::nullopt;
    if ( not value.is<T>() )
        throw std::runtime_error( std::format( "spawn: field '{}' has the wrong type", key ) );
    return value.as<T>();
}

// Same field under either of two names, so `pos` and `position` both work.
template <class T>
static std::optional<T> Field( const sol::table& table, const char* key, const char* alias )
{
    if ( auto value = Field<T>( table, key ) )
        return value;
    return Field<T>( table, alias );
}


// A resource field that takes either a path to load or an already-loaded
// handle, so `model = "cube.obj"` and `model = load_model("cube.obj")` both
// read naturally. Null when the field is absent.
template <class Resource, class Load>
static Ref<Resource> ResourceField( const sol::table& table,
                                    const char* key,
                                    Load load,
                                    string_view what )
{
    const sol::object value = table[key];
    if ( not value.valid() or value.is<sol::nil_t>() )
        return nullptr;
    if ( value.is<string>() )
        return LoadOrThrow<Ref<Resource>>( load, what, value.as<string>() );
    if ( value.is<Ref<Resource>>() )
        return value.as<Ref<Resource>>();

    throw std::runtime_error( std::format( "spawn: field '{}' must be a path or a loaded {}", key, what ) );
}


// The `light` field of a spawn table.
//
// Takes either an already-built Light - `light = Light.create_point_light()` -
// or a table naming exactly one of directional / point / spot, the way
// rigid_body names one of box / sphere / capsule:
//
//   light = { point = true, distance = 50, color = vec3( 1, 0.9, 0.8 ) }
//   light = { spot = true, distance = 30, cut_off = 15, outer_cut_off = 25 }
//
// The three checks below all cover cases that used to fail silently: a light
// with no type, a distance the attenuation table quietly clamps away, and a
// spot whose cones are inside out.
static std::optional<LightComponent> LightField( const sol::table& description )
{
    const sol::object value = description["light"];
    if ( not value.valid() or value.is<sol::nil_t>() )
        return std::nullopt;

    // A Light built by the factories still works, so existing scripts are fine.
    if ( value.is<LightComponent>() )
        return value.as<LightComponent>();

    if ( not value.is<sol::table>() )
        throw std::runtime_error( "spawn: field 'light' must be a Light or a table" );

    const sol::table table = value.as<sol::table>();

    const int directional = Field<bool>( table, "directional" ).value_or( false ) ? 1 : 0;
    const int point = Field<bool>( table, "point" ).value_or( false ) ? 1 : 0;
    const int spot = Field<bool>( table, "spot" ).value_or( false ) ? 1 : 0;
    if ( directional + point + spot != 1 )
        throw std::runtime_error( "spawn: light needs exactly one of directional, point or spot" );

    // Engine::OnUpdate rewrites both from the entity's transform every frame,
    // so a value given here would be gone by the first tick. Say so rather
    // than accepting it and ignoring it.
    for ( const char* owned : { "position", "direction" } )
    {
        const sol::object given = table[owned];
        if ( given.valid() and not given.is<sol::nil_t>() )
            throw std::runtime_error( std::format(
                "spawn: light '{}' comes from the entity transform - set pos/rot on the spawn instead", owned ) );
    }

    LightComponent light = directional ? LightComponent::CreateDirLight()
                         : point       ? LightComponent::CreatePointLight()
                                       : LightComponent::CreateSpotLight();

    if ( const auto color = Field<vec3>( table, "color" ) )
        light.mColor = *color;

    if ( const auto brightness = Field<double>( table, "brightness" ) )
        light.mBrightness = (f32)*brightness;

    if ( const auto distance = Field<double>( table, "distance" ) )
    {
        // Light::Update derives the attenuation constants from a lookup table
        // that only spans this range, and clamps outside it - which reads as
        // the distance simply not having taken effect.
        if ( *distance < 7.0 or *distance > 3250.0 )
            throw std::runtime_error( std::format(
                "spawn: light distance {} is outside the supported 7..3250 m range", *distance ) );
        light.mDistance = (f32)*distance;
    }

    if ( spot )
    {
        if ( const auto cutOff = Field<double>( table, "cut_off" ) )
            light.mCutOff = (f32)*cutOff;
        if ( const auto outerCutOff = Field<double>( table, "outer_cut_off" ) )
            light.mOuterCutOff = (f32)*outerCutOff;

        // The shader ramps across the gap between the two cosines. An outer
        // cone inside the inner one flips that ramp instead of softening it.
        if ( light.mOuterCutOff < light.mCutOff )
            throw std::runtime_error( std::format(
                "spawn: light outer_cut_off ({}) must be >= cut_off ({})",
                light.mOuterCutOff, light.mCutOff ) );
    }

    return light;
}


void CreateSpawnBindings( Scene& scene,
                          Loader& loader,
                          PhysicsEngine& physicsEngine,
                          sol::state& lua )
{
    // One entity from one table. Everything is optional and everything has the
    // obvious default, so the common case - put a cube over there - is a single
    // call instead of the eight-line create/add/add/add/add sequence that every
    // spawn function in every project ends up repeating.
    //
    //   spawn{
    //       tag   = "cube",
    //       pos   = vec3( 0, 5, 0 ),
    //       model = "models/cube/cube.obj",
    //       rigid_body = { box = vec3( 0.5 ), mass = 1, friction = 1.0 },
    //   }
    lua["spawn"] = [&]( const sol::table& description ) -> Entity
    {
        const Entity entity = scene.CreateEntity();

        if ( const auto tag = Field<string>( description, "tag" ) )
            scene.AddComponent<TagComponent>( entity, *tag );

        // A transform always exists. Nothing without one is drawn, simulated or
        // picked, and defaulting it costs nothing.
        const Transform transform( Field<vec3>( description, "pos", "position" ).value_or( vec3( 0 ) ),
                                   Field<vec3>( description, "rot", "rotation" ).value_or( vec3( 0 ) ),
                                   Field<vec3>( description, "scale" ).value_or( vec3( 1 ) ) );
        scene.AddComponent<TransformComponent>( entity, transform );

        if ( const auto model = ResourceField<Model>( description, "model",
                                                      [&]( const path& p ){ return loader.LoadModel( p ); },
                                                      "model" ) )
            scene.AddComponent<ModelComponent>( entity, model );

        // No shader key is not an error: the renderer draws a model with no
        // ShaderComponent using the default shader.
        if ( const auto shader = ResourceField<Shader>( description, "shader",
                                                        [&]( const path& p ){ return loader.LoadShader( p ); },
                                                        "shader" ) )
        {
            auto& component = scene.AddComponent<ShaderComponent>( entity, shader );
            component.RebuildUniforms( lua );
        }

        // Camera and Light are the names CameraComponent and LightComponent are
        // registered under - the bare Camera and Light are not usertypes, so a
        // script never holds one.
        if ( const auto camera = Field<CameraComponent>( description, "camera" ) )
            scene.AddComponent<CameraComponent>( entity, *camera );

        if ( const auto light = LightField( description ) )
            scene.AddComponent<LightComponent>( entity, *light );

        if ( const auto body = Field<sol::table>( description, "rigid_body" ) )
        {
            const f32 mass = (f32)Field<double>( *body, "mass" ).value_or( 0.0 );

            std::optional<RigidBody> rigidBody;
            if ( const auto halfExtent = Field<vec3>( *body, "box" ) )
                rigidBody = RigidBody::CreateBox( mass, *halfExtent );
            else if ( const auto radius = Field<double>( *body, "sphere" ) )
                rigidBody = RigidBody::CreateSphere( mass, (f32)*radius );
            else if ( const auto capsule = Field<sol::table>( *body, "capsule" ) )
                rigidBody = RigidBody::CreateCapsule( mass,
                                                      (f32)Field<double>( *capsule, "radius" ).value_or( 0.5 ),
                                                      (f32)Field<double>( *capsule, "height" ).value_or( 1.0 ) );
            else
                throw std::runtime_error( "spawn: rigid_body needs one of box, sphere or capsule" );

            rigidBody->SetTransform( transform.mPosition, transform.mRotation );
            if ( const auto friction = Field<double>( *body, "friction" ) )
                rigidBody->SetFriction( (f32)*friction );

            // A platform is scripted, not simulated. Bullet only treats a body
            // as kinematic when it is massless, so say that here rather than
            // silently ignoring a mass that was asked for.
            if ( Field<bool>( *body, "kinematic" ).value_or( false ) )
            {
                if ( mass != 0.0f )
                    throw std::runtime_error( "spawn: a kinematic rigid_body must have mass 0" );
                rigidBody->SetKinematic( true );
            }

            auto& component = scene.AddComponent<RigidBodyComponent>( entity, std::move( *rigidBody ) );
            physicsEngine.Add( component.mRigidBody, entity );
        }

        if ( const auto controller = Field<sol::table>( description, "character_controller" ) )
        {
            auto& component = scene.AddComponent<CharacterControllerComponent>(
                entity,
                (f32)Field<double>( *controller, "radius" ).value_or( 0.5 ),
                (f32)Field<double>( *controller, "height" ).value_or( 2.0 ),
                (f32)Field<double>( *controller, "step_height" ).value_or( 0.35 ) );
            component.mController.Warp( transform.mPosition );
            physicsEngine.Add( component.mController, entity );
        }

        // State before script: on_start runs below and is handed this table.
        scene.AddComponent<StateComponent>(
            entity, Any( Field<Table>( description, "state" ).value_or( lua.create_table() ) ) );

        if ( const auto script = ResourceField<Script>( description, "script",
                                                        [&]( const path& p ){ return loader.LoadScript( p ); },
                                                        "script" ) )
        {
            auto& component = scene.AddComponent<ScriptComponent>( entity, script );
            auto callbacks = ExtractScriptCallbacks( lua, script );
            component.mOnStart = std::move( callbacks.mOnStart );
            component.mOnUpdate = std::move( callbacks.mOnUpdate );
            CallScriptOnStart( component.mOnStart, script, entity,
                               *scene.GetComponent<StateComponent>( entity ).mState );
        }

        return entity;
    };
}

}
