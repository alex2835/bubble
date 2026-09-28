#include "engine/pch/pch.hpp"
#include "engine/scripting/bindings/scene_lua_bindings.hpp"
#include "binding_utils.hpp"
#include "engine/scene/component_manager.hpp"
#include "engine/scene/scene.hpp"
#include "engine/scene/node_path.hpp"
#include "engine/scene/hierarchy.hpp"
#include "engine/loader/loader.hpp"
#include "engine/physics/physics_engine.hpp"
#include "engine/scripting/scripting_engine.hpp"
#include <sol/sol.hpp>
#include <print>
#include "engine/scene/components/camera_component.hpp"
#include "engine/scene/components/character_controller_component.hpp"
#include "engine/scene/components/light_component.hpp"
#include "engine/scene/components/model_component.hpp"
#include "engine/scene/components/rigid_body_component.hpp"
#include "engine/scene/components/shader_component.hpp"
#include "engine/scene/components/tag_component.hpp"
#include "engine/scene/components/transform_component.hpp"
#include "engine/scene/components/audio_listener_component.hpp"
#include "engine/scene/components/audio_source_component.hpp"
#include "engine/scene/components/script_component.hpp"
#include "engine/scene/components/state_component.hpp"
#include "engine/scene/components/animator_component.hpp"

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


// Adding a physics component to an entity that already has one makes the
// scene replace it in place, which destroys the old Bullet body while the dynamics
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


namespace
{
// A component a script reached for. Missing, the error says whose and which
// - "'/player/player' has no Shader component" - where the registry's own
// would give an id the editor does not show.
template <typename Component>
Component& Need( Scene& scene, Entity entity )
{
    if ( not scene.HasEntity( entity ) )
        throw std::runtime_error( std::format( "{} is used after it was removed", DescribeEntity( scene, entity ) ) );
    if ( not scene.HasComponent<Component>( entity ) )
        throw std::runtime_error( std::format( "{} has no {} component", DescribeEntity( scene, entity ), Component::Name() ) );
    return scene.GetComponent<Component>( entity );
}

// find(): the entity at `path`, or an error that says which part is missing
// where and what is there instead.
Entity FindOrThrow( const Scene& scene, Entity from, const string& path )
{
    const Entity found = FindByPath( scene, from, path );
    if ( found == Entity::Null )
        throw std::runtime_error( std::format( "find( \"{}\" ): {}", path, WhyPathFails( scene, from, path ) ) );
    return found;
}
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

    // NodePath( "../door" ): an entity by its place in the tree, for State
    // tables - see node_path.hpp. A level's are entities by the time scripts
    // run; one made in a script is turned into an entity with entity:find().
    lua.new_usertype<NodePath>(
        "NodePath",
        sol::call_constructor,
        sol::constructors<NodePath(), NodePath( string )>(),
        "path", sol::readonly( &NodePath::mPath ),
        sol::meta_function::to_string,
        []( const NodePath& p ) { return std::format( "NodePath({})", p.mPath ); } );

    // Entity
    lua.new_usertype<Entity>(
        "Entity",
        sol::meta_function::to_string,
        []( const Entity& entity ){ return std::to_string( (size_t)entity ); },
        
        // Add
        "add_tag",
        sol::overload(
            // The name made unique among the entity's siblings, as in the
            // editor: a second "enemy" is enemy2.
            [&]( const Entity& entity, const string& tag )
            {
                scene.AddComponent<TagComponent>( entity, tag );
                MakeNameUnique( scene, entity );
            },
            [&]( const Entity& entity, const TagComponent& c )
            {
                scene.AddComponent<TagComponent>( entity, c );
                MakeNameUnique( scene, entity );
            }
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
        // Both forms take the entity's transform straight away - see
        // SyncToEntityTransform. Without it the light spends its first frame at
        // the origin with the attenuation of the default distance.
        "add_light",
        sol::overload(
            [&]( const Entity& entity, const Light& light )
            {
                scene.AddComponent<LightComponent>( entity, light );
                SyncToEntityTransform<LightComponent>( scene, entity );
            },
            [&]( const Entity& entity, const LightComponent& c )
            {
                scene.AddComponent<LightComponent>( entity, c );
                SyncToEntityTransform<LightComponent>( scene, entity );
            }
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
        // Likewise: play() reads the position cached on the component, so a
        // script that creates a source and plays it in the same call would
        // otherwise start the sound at the origin.
        "add_audio_source",
        sol::overload(
            [&]( const Entity& entity, const string& soundPath )
            {
                scene.AddComponent<AudioSourceComponent>(
                    entity, LoadOrThrow<Ref<Sound>>( [&]( const path& p ){ return loader.LoadSound( p ); },
                                                     "sound", soundPath ) );
                SyncToEntityTransform<AudioSourceComponent>( scene, entity );
            },
            [&]( const Entity& entity, const Ref<Sound>& sound )
            {
                scene.AddComponent<AudioSourceComponent>( entity, sound );
                SyncToEntityTransform<AudioSourceComponent>( scene, entity );
            },
            [&]( const Entity& entity )
            {
                scene.AddComponent<AudioSourceComponent>( entity );
                SyncToEntityTransform<AudioSourceComponent>( scene, entity );
            }
        ),
        "add_audio_listener",
        sol::overload(
            [&]( const Entity& entity ) { scene.AddComponent<AudioListenerComponent>( entity ); },
            [&]( const Entity& entity, const AudioListenerComponent& c ) { scene.AddComponent<AudioListenerComponent>( entity, c ); }
        ),
        "add_animator",
        sol::overload(
            [&]( const Entity& entity ) { scene.AddComponent<AnimatorComponent>( entity ); },
            [&]( const Entity& entity, const string& clip ) { scene.AddComponent<AnimatorComponent>( entity ).Play( clip ); },
            [&]( const Entity& entity, const AnimatorComponent& c ) { scene.AddComponent<AnimatorComponent>( entity, c ); }
        ),
        // The controller is a project resource, so attaching one needs the
        // loader; the Animator usertype has none.
        "set_animation_controller",
        [&]( const Entity& entity, const string& controllerPath )
        {
            auto controller = LoadOrThrow<Ref<AnimationController>>(
                [&]( const path& p ){ return loader.LoadAnimationController( p ); }, "animation controller", controllerPath );
            Need<AnimatorComponent>( scene, entity ).SetController( controller );
        },
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
            CallScriptOnStart( component.mOnStart, script, scene, entity,
                               *Need<StateComponent>( scene, entity ).mState );
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
        [&]( const Entity& entity ) -> TagComponent& { return Need<TagComponent>( scene, entity ); },
        "get_transform",
        [&]( const Entity& entity ) -> TransformComponent& { return Need<TransformComponent>( scene, entity ); },
        "get_model",
        [&]( const Entity& entity ) -> ModelComponent& { return Need<ModelComponent>( scene, entity ); },
        "get_shader",
        [&]( const Entity& entity ) -> ShaderComponent& { return Need<ShaderComponent>( scene, entity ); },
        "get_camera",
        [&]( const Entity& entity ) -> CameraComponent& { return Need<CameraComponent>( scene, entity ); },
        "get_light",
        [&]( const Entity& entity ) -> LightComponent& { return Need<LightComponent>( scene, entity ); },
        "get_rigid_body",
        [&]( const Entity& entity ) -> RigidBody& { return Need<RigidBodyComponent>( scene, entity ).mRigidBody; },
        "get_character_controller",
        [&]( const Entity& entity ) -> CharacterController& { return Need<CharacterControllerComponent>( scene, entity ).mController; },
        "get_audio_source",
        [&]( const Entity& entity ) -> AudioSourceComponent& { return Need<AudioSourceComponent>( scene, entity ); },
        "get_audio_listener",
        [&]( const Entity& entity ) -> AudioListenerComponent& { return Need<AudioListenerComponent>( scene, entity ); },
        "get_animator",
        [&]( const Entity& entity ) -> AnimatorComponent& { return Need<AnimatorComponent>( scene, entity ); },
        "get_state",
        [&]( const Entity& entity ) -> Any { return *Need<StateComponent>( scene, entity ).mState; },

        // Whether this handle still names a live entity.
        //
        // Entity ids come from a counter and are never reused, so a handle that
        // has gone stale stays stale - it can never quietly start referring to
        // a different entity. That is what makes it safe for a script to keep
        // handles in `state` across frames and test them here, which is the
        // only supported way to hold on to anything from the scene.
        "is_valid",
        [&]( const Entity& entity ) -> bool { return scene.HasEntity( entity ); },

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
        "has_audio_source",
        [&]( const Entity& entity ) ->bool { return scene.HasComponent<AudioSourceComponent>( entity ); },
        "has_audio_listener",
        [&]( const Entity& entity ) ->bool { return scene.HasComponent<AudioListenerComponent>( entity ); },
        "has_animator",
        [&]( const Entity& entity ) ->bool { return scene.HasComponent<AnimatorComponent>( entity ); },
        "has_state",
        [&]( const Entity& entity ) ->bool { return scene.HasComponent<StateComponent>( entity ); },

        // Shorthands for the chains that show up in every script.
        // entity:get_transform().position and entity:get_shader().uniforms are
        // three quarters of what a gameplay script does with an entity.
        "position",
        sol::property(
            [&]( const Entity& entity ) { return Need<TransformComponent>( scene, entity ).mPosition; },
            [&]( const Entity& entity, const vec3& v ) { Need<TransformComponent>( scene, entity ).mPosition = v; }
        ),
        "rotation",
        sol::property(
            [&]( const Entity& entity ) { return Need<TransformComponent>( scene, entity ).Euler(); },
            [&]( const Entity& entity, const vec3& v ) { Need<TransformComponent>( scene, entity ).SetEuler( v ); }
        ),
        "scale",
        sol::property(
            [&]( const Entity& entity ) { return Need<TransformComponent>( scene, entity ).mScale; },
            [&]( const Entity& entity, const vec3& v ) { Need<TransformComponent>( scene, entity ).mScale = v; }
        ),
        "uniforms",
        sol::property(
            [&]( const Entity& entity ) -> sol::object
            {
                auto& component = Need<ShaderComponent>( scene, entity );
                component.EnsureUniforms( lua );
                if ( not component.mUniforms )
                    return sol::make_object( lua, sol::lua_nil );
                return sol::object( component.mUniforms->as<Table>() );
            }
        ),
        "state",
        sol::property(
            [&]( const Entity& entity ) -> Any { return *Need<StateComponent>( scene, entity ).mState; }
        ),

        // The hierarchy. position / rotation / scale above are relative to
        // the parent; these are where the entity is in the world, as of the
        // last world update (after physics, and again after the scripts).
        "world_position",
        sol::property(
            [&]( const Entity& entity ) { return Need<TransformComponent>( scene, entity ).World().mPosition; }
        ),
        "world_rotation",
        sol::property(
            [&]( const Entity& entity ) { return Need<TransformComponent>( scene, entity ).World().Euler(); }
        ),
        "get_parent",
        [&]( const Entity& entity ) -> opt<Entity>
        {
            const Entity parent = ParentOf( scene, entity );
            return parent == Entity::Null ? std::nullopt : opt<Entity>( parent );
        },
        // nil puts it under the level's root. keep_world (default true)
        // leaves it where it is in the world; false keeps its local
        // transform, so it jumps to the same place relative to the new
        // parent. False on a loop.
        "set_parent",
        [&]( const Entity& entity, sol::object parent, sol::optional<bool> keepWorld ) -> bool
        {
            const Entity target = parent.is<Entity>() ? parent.as<Entity>() : Entity::Null;
            if ( not SetParent( scene, entity, target, keepWorld.value_or( true ) ) )
                return false;
            MakeNameUnique( scene, entity );
            return true;
        },
        "get_children",
        [&]( const Entity& entity )
        {
            const auto children = ChildrenOf( scene, entity );
            return sol::as_table( vector<Entity>( children.begin(), children.end() ) );
        },
        // By a path of names from this entity: "wheel", "../door",
        // "/player/camera" (from the level's root), "~/camera" (from the
        // root of the prefab instance it is in). An error that says what is
        // missing where when nothing is there; try_find gives nil instead.
        "find",
        [&]( const Entity& entity, const string& path ) { return FindOrThrow( scene, entity, path ); },
        "try_find",
        [&]( const Entity& entity, const string& path ) -> opt<Entity>
        {
            const Entity found = FindByPath( scene, entity, path );
            return found == Entity::Null ? std::nullopt : opt<Entity>( found );
        },
        // "/player/camera".
        "get_path",
        [&]( const Entity& entity ) { return PathOf( scene, entity ); },
        // The root of the prefab instance the entity is in - what "~" means.
        "get_prefab_root",
        [&]( const Entity& entity ) { return PrefabRootOf( scene, entity ); },
        // The Tag's name. Set, it is made unique among the siblings.
        "name",
        sol::property(
            [&]( const Entity& entity ) { return NameOf( scene, entity ); },
            [&]( const Entity& entity, const string& name )
            {
                if ( not scene.HasComponent<TagComponent>( entity ) )
                    scene.AddComponent<TagComponent>( entity );
                Need<TagComponent>( scene, entity ).mName = name;
                MakeNameUnique( scene, entity );
            }
        )
    );

    // Scene
    // Made under the level's root, so it is in the tree like everything else.
    lua["create_entity"] = [&](){ return CreateChildEntity( scene ); };

    // The level's tree from the top: level:find( "props/chair" ) (an error
    // when nothing is there), level:try_find( ... ) (nil), level:root().
    lua["level"] = lua.create_table_with(
        "find", [&]( sol::object, const string& path ) { return FindOrThrow( scene, scene.Root(), path ); },
        "try_find", [&]( sol::object, const string& path ) -> opt<Entity>
        {
            const Entity found = FindByPath( scene, scene.Root(), path );
            return found == Entity::Null ? std::nullopt : opt<Entity>( found );
        },
        "root", [&]( sol::object ) { return scene.Root(); } );


    lua["remove_entity"] = [&]( Entity entity ) {
        // Scene::RemoveEntity throws on an unknown entity; this names the call
        // a stale handle held by a script came through.
        if ( not scene.HasEntity( entity ) )
            throw std::runtime_error( std::format( "remove_entity: no such entity {}", (size_t)entity ) );

        if ( entity == scene.Root() )
            throw std::runtime_error( "remove_entity: the level's root stays" );

        // With everything under it, as deleting it in the editor does.
        const vector<Entity> subtree = Subtree( scene, entity );
        DetachFromParent( scene, entity );
        for ( const Entity e : subtree )
        {
            DetachRigidBody( scene, physicsEngine, e );
            DetachCharacterController( scene, physicsEngine, e );
        }
        scene.RemoveEntities( subtree );
    };

}

} // namespace bubble