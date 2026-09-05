#include "engine/pch/pch.hpp"
#include "engine/scripting/bindings/scene_lua_bindings.hpp"
#include "binding_utils.hpp"
#include "engine/scene/component_manager.hpp"
#include "engine/scene/scene.hpp"
#include "engine/loader/loader.hpp"
#include "engine/physics/physics_engine.hpp"
#include "engine/scripting/scripting_engine.hpp"
#include <sol/sol.hpp>
#include <print>

namespace bubble
{
// Scripts address components as Component.tag, Component.transform, ... A
// hand-written copy of the ids would desync from ComponentID silently, and a
// script would then iterate the wrong pool, so build the table from the enum.
static string BuildComponentEnum()
{
    string source = "Component =\n{\n";
    for ( const auto& [value, name] : magic_enum::enum_entries<ComponentID>() )
        source += std::format( "    {} = {},\n", ComponentLuaName( value ), static_cast<int>( value ) );
    source += "}\n";
    return source;
}


// Adding a physics component to an entity that already has one makes recs
// replace it in place, which destroys the old Bullet body while the dynamics
// world still holds a raw pointer to it. Take it out of the world before the
// component is overwritten - and for the same reason before the entity is
// removed, since the component is destroyed either way.
//
// These are free functions, not lambdas local to CreateSceneBindings. The
// binding closures below outlive that call, so capturing a local lambda by
// reference would leave every one of them pointing into a dead stack frame.
static void DetachRigidBody( Scene& scene, PhysicsEngine& physicsEngine, const Entity& entity )
{
    if ( scene.HasComponent<RigidBodyComponent>( entity ) )
        physicsEngine.Remove( scene.GetComponent<RigidBodyComponent>( entity ).mRigidBody );
}

static void DetachCharacterController( Scene& scene, PhysicsEngine& physicsEngine, const Entity& entity )
{
    if ( scene.HasComponent<CharacterControllerComponent>( entity ) )
        physicsEngine.Remove( scene.GetComponent<CharacterControllerComponent>( entity ).mController );
}


void CreateSceneBindings( Scene& scene,
                          Loader& loader,
                          PhysicsEngine& physicsEngine,
                          sol::state& lua )
{
    lua.script( BuildComponentEnum() );

    // Component bindings
    for ( const auto& [name, compFuncTable] : ComponentManager::Instance() )
        compFuncTable.mCreateLuaBinding( lua );

    // Entity
    lua.new_usertype<Entity>(
        "Entity",
        sol::meta_function::to_string,
        []( const Entity& entity ){ return std::to_string( (size_t)entity ); },
        
        // Add
        "add_tag",
        sol::overload(
            [&]( const Entity& entity, const string& tag ) { scene.AddComponent<TagComponent>( entity, tag ); },
            [&]( const Entity& entity, const TagComponent& c ) { scene.AddComponent<TagComponent>( entity, c ); }
        ),
        // The no-argument and vec3 forms are additions: a transform is required
        // for an entity to be drawn at all, and spelling out
        // Transform(pos, vec3(0), vec3(1)) to get the obvious defaults is the
        // single most repeated line in a spawn script.
        "add_transform",
        sol::overload(
            [&]( const Entity& entity ) { scene.AddComponent<TransformComponent>( entity ); },
            [&]( const Entity& entity, const vec3& position )
            { scene.AddComponent<TransformComponent>( entity, Transform( position ) ); },
            [&]( const Entity& entity, const vec3& position, const vec3& rotation )
            { scene.AddComponent<TransformComponent>( entity, Transform( position, rotation ) ); },
            [&]( const Entity& entity, const vec3& position, const vec3& rotation, const vec3& scale )
            { scene.AddComponent<TransformComponent>( entity, Transform( position, rotation, scale ) ); },
            [&]( const Entity& entity, const Transform& t ) { scene.AddComponent<TransformComponent>( entity, t ); },
            [&]( const Entity& entity, const TransformComponent& c ) { scene.AddComponent<TransformComponent>( entity, c ); }
        ),
        // The string forms are additions; load_model(...) still works and is
        // still the way to hold on to a model and reuse it.
        "add_model",
        sol::overload(
            [&]( const Entity& entity, const string& modelPath )
            {
                scene.AddComponent<ModelComponent>(
                    entity, LoadOrThrow<Ref<Model>>( [&]( const path& p ){ return loader.LoadModel( p ); },
                                                     "model", modelPath ) );
            },
            [&]( const Entity& entity, const Ref<Model>& model ) { scene.AddComponent<ModelComponent>( entity, model ); },
            [&]( const Entity& entity, const ModelComponent& c ) { scene.AddComponent<ModelComponent>( entity, c ); }
        ),
        // Every form rebuilds the uniform table. A ShaderComponent built
        // straight from a Ref<Shader> has none, and everything that reads one -
        // the `uniforms` property, the inspector, the draw loop - assumed one
        // was there, so a shader attached from a script had no settable
        // uniforms at all.
        "add_shader",
        sol::overload(
            [&]( const Entity& entity, const string& shaderPath )
            {
                auto& component = scene.AddComponent<ShaderComponent>(
                    entity, LoadOrThrow<Ref<Shader>>( [&]( const path& p ){ return loader.LoadShader( p ); },
                                                      "shader", shaderPath ) );
                component.RebuildUniforms( lua );
            },
            [&]( const Entity& entity, const Ref<Shader>& shader )
            {
                auto& component = scene.AddComponent<ShaderComponent>( entity, shader );
                component.RebuildUniforms( lua );
            },
            [&]( const Entity& entity, const ShaderComponent& c )
            {
                auto& component = scene.AddComponent<ShaderComponent>( entity, c );
                component.EnsureUniforms( lua );
            }
        ),
        "add_camera",
        sol::overload(
            [&]( const Entity& entity, const Camera& camera ) { scene.AddComponent<CameraComponent>( entity, camera ); },
            [&]( const Entity& entity, const CameraComponent& c ) { scene.AddComponent<CameraComponent>( entity, c ); }
        ),
        "add_light",
        sol::overload(
            [&]( const Entity& entity, const Light& light ) { scene.AddComponent<LightComponent>( entity, light ); },
            [&]( const Entity& entity, const LightComponent& c ) { scene.AddComponent<LightComponent>( entity, c ); }
        ),
        "add_rigid_body",
        sol::overload(
            [&]( const Entity& entity, RigidBody object )
            {
                DetachRigidBody( scene, physicsEngine, entity );
                auto& c = scene.AddComponent<RigidBodyComponent>( entity, std::move( object ) );
                physicsEngine.Add( c.mRigidBody, entity );
            },
            [&]( const Entity& entity, RigidBodyComponent comp )
            {
                DetachRigidBody( scene, physicsEngine, entity );
                auto& c = scene.AddComponent<RigidBodyComponent>( entity, std::move( comp ) );
                physicsEngine.Add( c.mRigidBody, entity );
            }
        ),
        "add_character_controller",
        sol::overload(
            [&]( const Entity& entity, f32 radius, f32 height, f32 stepHeight )
            {
                DetachCharacterController( scene, physicsEngine, entity );
                auto& c = scene.AddComponent<CharacterControllerComponent>( entity, radius, height, stepHeight );
                physicsEngine.Add( c.mController, entity );
            },
            [&]( const Entity& entity, CharacterControllerComponent comp )
            {
                DetachCharacterController( scene, physicsEngine, entity );
                auto& c = scene.AddComponent<CharacterControllerComponent>( entity, std::move( comp ) );
                physicsEngine.Add( c.mController, entity );
            }
        ),
        "add_state",
        sol::overload(
            [&]( const Entity& entity ) { scene.AddComponent<StateComponent>( entity ); },
            [&]( const Entity& entity, Any object ) { scene.AddComponent<StateComponent>( entity, object ); }
        ),
        // Attaching a script at runtime. The state component comes with it -
        // every callback is handed one, and a script on an entity without one
        // used to be skipped by the update loop without a word. on_start runs
        // immediately, because the entity is already live.
        "add_script",
        [&]( const Entity& entity, const string& scriptPath )
        {
            auto script = LoadOrThrow<Ref<Script>>( [&]( const path& p ){ return loader.LoadScript( p ); },
                                                    "script", scriptPath );
            if ( not scene.HasComponent<StateComponent>( entity ) )
                scene.AddComponent<StateComponent>( entity );

            auto& component = scene.AddComponent<ScriptComponent>( entity, script );
            auto callbacks = ExtractScriptCallbacks( lua, script );
            component.mOnStart = std::move( callbacks.mOnStart );
            component.mOnUpdate = std::move( callbacks.mOnUpdate );
            CallScriptOnStart( component.mOnStart, script, entity,
                               *scene.GetComponent<StateComponent>( entity ).mState );
        },

        // Get — one name per component. Each returns the type that actually
        // carries the fields a script wants.
        //
        // For most components that is the *Component itself: TransformComponent,
        // CameraComponent and LightComponent derive from Transform/Camera/Light,
        // and only the derived type is registered as a usertype, so returning a
        // base reference would push userdata with no accessible members at all
        // (indexing it raises "attempt to index a sol.Camera * value").
        //
        // RigidBodyComponent and CharacterControllerComponent instead *contain*
        // their payload and expose nothing else, so the inner object - the one
        // holding jump(), set_walk_direction(), set_friction() - is what a script
        // needs.
        "get_tag",
        [&]( const Entity& entity ) -> TagComponent& { return scene.GetComponent<TagComponent>( entity ); },
        "get_transform",
        [&]( const Entity& entity ) -> TransformComponent& { return scene.GetComponent<TransformComponent>( entity ); },
        "get_model",
        [&]( const Entity& entity ) -> ModelComponent& { return scene.GetComponent<ModelComponent>( entity ); },
        "get_shader",
        [&]( const Entity& entity ) -> ShaderComponent& { return scene.GetComponent<ShaderComponent>( entity ); },
        "get_camera",
        [&]( const Entity& entity ) -> CameraComponent& { return scene.GetComponent<CameraComponent>( entity ); },
        "get_light",
        [&]( const Entity& entity ) -> LightComponent& { return scene.GetComponent<LightComponent>( entity ); },
        "get_rigid_body",
        [&]( const Entity& entity ) -> RigidBody& { return scene.GetComponent<RigidBodyComponent>( entity ).mRigidBody; },
        "get_character_controller",
        [&]( const Entity& entity ) -> CharacterController& { return scene.GetComponent<CharacterControllerComponent>( entity ).mController; },
        "get_state",
        [&]( const Entity& entity ) -> Any { return *scene.GetComponent<StateComponent>( entity ).mState; },

        // Has
        "has_tag",
        [&]( const Entity& entity ) ->bool { return scene.HasComponent<TagComponent>( entity ); },
        "has_transform",
        [&]( const Entity& entity ) ->bool { return scene.HasComponent<TransformComponent>( entity ); },
        "has_model",
        [&]( const Entity& entity ) ->bool { return scene.HasComponent<ModelComponent>( entity ); },
        "has_shader",
        [&]( const Entity& entity ) ->bool { return scene.HasComponent<ShaderComponent>( entity ); },
        "has_camera",
        [&]( const Entity& entity ) ->bool { return scene.HasComponent<CameraComponent>( entity ); },
        "has_light",
        [&]( const Entity& entity ) ->bool { return scene.HasComponent<LightComponent>( entity ); },
        "has_rigid_body",
        [&]( const Entity& entity ) ->bool { return scene.HasComponent<RigidBodyComponent>( entity ); },
        "has_character_controller",
        [&]( const Entity& entity ) ->bool { return scene.HasComponent<CharacterControllerComponent>( entity ); },
        "has_state",
        [&]( const Entity& entity ) ->bool { return scene.HasComponent<StateComponent>( entity ); },

        // Shorthands for the chains that show up in every script.
        // entity:get_transform().position and entity:get_shader().uniforms are
        // three quarters of what a gameplay script does with an entity.
        "position",
        sol::property(
            [&]( const Entity& entity ) { return scene.GetComponent<TransformComponent>( entity ).mPosition; },
            [&]( const Entity& entity, const vec3& v ) { scene.GetComponent<TransformComponent>( entity ).mPosition = v; }
        ),
        "rotation",
        sol::property(
            [&]( const Entity& entity ) { return scene.GetComponent<TransformComponent>( entity ).mRotation; },
            [&]( const Entity& entity, const vec3& v ) { scene.GetComponent<TransformComponent>( entity ).mRotation = v; }
        ),
        "scale",
        sol::property(
            [&]( const Entity& entity ) { return scene.GetComponent<TransformComponent>( entity ).mScale; },
            [&]( const Entity& entity, const vec3& v ) { scene.GetComponent<TransformComponent>( entity ).mScale = v; }
        ),
        "uniforms",
        sol::property(
            [&]( const Entity& entity ) -> sol::object
            {
                auto& component = scene.GetComponent<ShaderComponent>( entity );
                component.EnsureUniforms( lua );
                if ( not component.mUniforms )
                    return sol::make_object( lua, sol::lua_nil );
                return sol::object( component.mUniforms->as<Table>() );
            }
        ),
        "state",
        sol::property(
            [&]( const Entity& entity ) -> Any { return *scene.GetComponent<StateComponent>( entity ).mState; }
        )
    );

    // Scene
    lua["create_entity"] = [&](){ return scene.CreateEntity(); };


    lua["remove_entity"] = [&]( Entity entity ) {
        // Registry::RemoveEntity asserts on an unknown entity, so a stale handle
        // held by a script would take a debug build down.
        if ( not scene.HasEntity( entity ) )
            throw std::runtime_error( std::format( "remove_entity: no such entity {}", (size_t)entity ) );

        DetachRigidBody( scene, physicsEngine, entity );
        DetachCharacterController( scene, physicsEngine, entity );
        scene.RemoveEntity( entity );
    };

}

} // namespace bubble