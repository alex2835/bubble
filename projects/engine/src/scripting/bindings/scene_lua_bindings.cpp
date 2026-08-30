#include "engine/pch/pch.hpp"
#include "engine/scripting/bindings/scene_lua_bindings.hpp"
#include "engine/scene/component_manager.hpp"
#include "engine/scene/scene.hpp"
#include <sol/sol.hpp>
#include <print>

namespace bubble
{
// The Lua API is snake_case throughout, while ComponentID is PascalCase.
static string ToSnakeCase( string_view name )
{
    string out;
    for ( size_t i = 0; i < name.size(); i++ )
    {
        const unsigned char c = (unsigned char)name[i];
        if ( std::isupper( c ) and i > 0 and not std::isupper( (unsigned char)name[i - 1] ) )
            out += '_';
        out += (char)std::tolower( c );
    }
    return out;
}

// Scripts address components as Component.tag, Component.transform, ... A
// hand-written copy of the ids would desync from ComponentID silently, and a
// script would then iterate the wrong pool, so build the table from the enum.
static string BuildComponentEnum()
{
    string source = "Component =\n{\n";
    for ( const auto& [value, name] : magic_enum::enum_entries<ComponentID>() )
        source += std::format( "    {} = {},\n", ToSnakeCase( name ), static_cast<int>( value ) );
    source += "}\n";
    return source;
}


void CreateSceneBindings( Scene& scene,
                          PhysicsEngine& physicsEngine,
                          sol::state& lua )
{
    lua.script( BuildComponentEnum() );

    // Component bindings
    for ( const auto& [name, compFuncTable] : ComponentManager::Instance() )
        compFuncTable.mCreateLuaBinding( lua );

    // Adding a physics component to an entity that already has one makes recs
    // replace it in place, which destroys the old Bullet body while the
    // dynamics world still holds a raw pointer to it. Take it out of the world
    // before the component is overwritten - and for the same reason before the
    // entity is removed, since the component is destroyed either way.
    auto detachRigidBody = [&]( const Entity& entity )
    {
        if ( scene.HasComponent<RigidBodyComponent>( entity ) )
            physicsEngine.Remove( scene.GetComponent<RigidBodyComponent>( entity ).mRigidBody );
    };

    auto detachCharacterController = [&]( const Entity& entity )
    {
        if ( scene.HasComponent<CharacterControllerComponent>( entity ) )
            physicsEngine.Remove( scene.GetComponent<CharacterControllerComponent>( entity ).mController );
    };

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
        "add_transform",
        sol::overload(
            [&]( const Entity& entity, const Transform& t ) { scene.AddComponent<TransformComponent>( entity, t ); },
            [&]( const Entity& entity, const TransformComponent& c ) { scene.AddComponent<TransformComponent>( entity, c ); }
        ),
        "add_model",
        sol::overload(
            [&]( const Entity& entity, const Ref<Model>& model ) { scene.AddComponent<ModelComponent>( entity, model ); },
            [&]( const Entity& entity, const ModelComponent& c ) { scene.AddComponent<ModelComponent>( entity, c ); }
        ),
        "add_shader",
        sol::overload(
            [&]( const Entity& entity, const Ref<Shader>& shader ) { scene.AddComponent<ShaderComponent>( entity, shader ); },
            [&]( const Entity& entity, const ShaderComponent& c ) { scene.AddComponent<ShaderComponent>( entity, c ); }
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
                detachRigidBody( entity );
                auto& c = scene.AddComponent<RigidBodyComponent>( entity, std::move( object ) );
                physicsEngine.Add( c.mRigidBody, entity );
            },
            [&]( const Entity& entity, RigidBodyComponent comp )
            {
                detachRigidBody( entity );
                auto& c = scene.AddComponent<RigidBodyComponent>( entity, std::move( comp ) );
                physicsEngine.Add( c.mRigidBody, entity );
            }
        ),
        "add_character_controller",
        sol::overload(
            [&]( const Entity& entity, f32 radius, f32 height, f32 stepHeight )
            {
                detachCharacterController( entity );
                auto& c = scene.AddComponent<CharacterControllerComponent>( entity, radius, height, stepHeight );
                physicsEngine.Add( c.mController, entity );
            },
            [&]( const Entity& entity, CharacterControllerComponent comp )
            {
                detachCharacterController( entity );
                auto& c = scene.AddComponent<CharacterControllerComponent>( entity, std::move( comp ) );
                physicsEngine.Add( c.mController, entity );
            }
        ),
        "add_state",
        [&]( const Entity& entity, Any object ) { scene.AddComponent<StateComponent>( entity, object ); },

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
        [&]( const Entity& entity ) ->bool { return scene.HasComponent<StateComponent>( entity ); }
    );

    // Scene
    lua["create_entity"] = [&](){ return scene.CreateEntity(); };

    lua["remove_entity"] = [&]( Entity entity ) {
        // Registry::RemoveEntity asserts on an unknown entity, so a stale handle
        // held by a script would take a debug build down.
        if ( not scene.HasEntity( entity ) )
            throw std::runtime_error( std::format( "remove_entity: no such entity {}", (size_t)entity ) );

        detachRigidBody( entity );
        detachCharacterController( entity );
        scene.RemoveEntity( entity );
    };

    lua["for_each_entity"] = [&]( const sol::table& components, const sol::function& func )
    {
        constexpr size_t componentsCount = magic_enum::enum_count<ComponentID>();
        using ComponentsIdsArray = std::array<ComponentTypeId, componentsCount>;
        using ComponentsDataArray = std::array<void*, componentsCount>;

        // Fill component ids. Every value here comes straight from a script, so
        // the table can be any length and hold anything at all - an unchecked
        // index would run off the end of componentsIds.
        ComponentsIdsArray componentsIds;
        componentsIds.fill( INVALID_COMPONENT_TYPE_ID );
        size_t idsCount = 0;
        for ( const auto& [k, v] : components )
        {
            if ( idsCount >= componentsCount )
                throw std::runtime_error( std::format( "for_each_entity: at most {} components expected",
                                                       componentsCount ) );
            if ( not v.is<int>() )
                throw std::runtime_error( "for_each_entity: expects an array of components like Component.tag" );

            const int componentId = v.as<int>();
            if ( not magic_enum::enum_contains<ComponentID>( componentId ) )
                throw std::runtime_error( std::format( "for_each_entity: unknown component id {}", componentId ) );

            componentsIds[idsCount++] = (ComponentTypeId)componentId;
        }

        // Same table during whole iterations
        auto componentsTable = lua.create_table( 0, componentsCount );

        scene.RuntimeForEach( componentsIds,
        [&]( Entity entity, ComponentsDataArray componentsData )
        {
            for ( size_t componentIdx = 0; componentIdx < componentsCount; componentIdx++ )
            {
                auto componentId = componentsIds[componentIdx];
                if ( componentId == INVALID_COMPONENT_TYPE_ID )
                    continue;

                auto componentDataPtr = componentsData[componentIdx];
                switch ( (ComponentID)componentId )
                {
                    case ComponentID::Tag:
                        componentsTable[ComponentID::Tag] = (TagComponent*)componentDataPtr;
                        break;
                    case ComponentID::Transform:
                        componentsTable[ComponentID::Transform] = (TransformComponent*)componentDataPtr;
                        break;
                    case ComponentID::Model:
                        componentsTable[ComponentID::Model] = (ModelComponent*)componentDataPtr;
                        break;
                    case ComponentID::Shader:
                        componentsTable[ComponentID::Shader] = (ShaderComponent*)componentDataPtr;
                        break;
                    case ComponentID::Script:
                        componentsTable[ComponentID::Script] = (ScriptComponent*)componentDataPtr;
                        break;
                    case ComponentID::RigidBody:
                        componentsTable[ComponentID::RigidBody] = (RigidBodyComponent*)componentDataPtr;
                        break;
                    case ComponentID::CharacterController:
                        componentsTable[ComponentID::CharacterController] = (CharacterControllerComponent*)componentDataPtr;
                        break;
                    case ComponentID::Camera:
                        componentsTable[ComponentID::Camera] = (CameraComponent*)componentDataPtr;
                        break;
                    case ComponentID::Light:
                        componentsTable[ComponentID::Light] = (LightComponent*)componentDataPtr;
                        break;
                    case ComponentID::State:
                        componentsTable[ComponentID::State] = *((StateComponent*)componentDataPtr)->mState;
                        break;
                    default:
                        throw std::runtime_error( "for_each_entity: invalid set of components provided" );
                }
            }
            func( entity, componentsTable );
        } );
    };
}

} // namespace bubble