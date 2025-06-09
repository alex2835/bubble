#include "engine/pch/pch.hpp"
#include "engine/scripting/bindings/scene_lua_bindings.hpp"
#include "engine/scene/component_manager.hpp"
#include "engine/scene/scene.hpp"
#include "engine/utils/filesystem.hpp"
#include <sol/sol.hpp>
#include <print>


namespace bubble
{
constexpr string_view componentEnum = R"(
Component = 
{
    Tag = 0,
    Transform = 1,
    Camera = 2,
    Model = 3,
    Light = 4,
    Shader = 5,
    Script = 6,
    Physics = 7,
    State = 8
}
)";


void CreateSceneBindings( Scene& scene,
                          PhysicsEngine& physicsEngine,
                          sol::state& lua )
{
    lua.script( componentEnum );

    // Component bindings
    for ( const auto& [name, compFuncTable] : ComponentManager::Instance() )
        compFuncTable.mCreateLuaBinding( lua );

    // Entity
    lua.new_usertype<Entity>(
        "Entity",
        sol::meta_function::to_string,
        []( const Entity& entity ){ return std::to_string( (size_t)entity ); },
        
        // Add
        "AddTagComponent",
        [&]( Entity& entity, string tag ) { scene.AddComponent<TagComponent>( entity, tag ); },
        "AddTransformComponent", 
        [&]( Entity& entity, TransformComponent trasform ) { scene.AddComponent<TransformComponent>( entity, trasform ); },
        "AddModelComponent",
        [&]( Entity& entity, Ref<Model> model ) { scene.AddComponent<ModelComponent>( entity, model ); },
        "AddShaderComponent",
        [&]( Entity& entity, Ref<Shader> shader ) { scene.AddComponent<ShaderComponent>( entity, shader ); },
        "AddPhysicsComponent",
        [&]( Entity& entity, const PhysicsObject& object ) 
        {
            auto& physicsComponent = scene.AddComponent<PhysicsComponent>( entity, object );
            physicsEngine.Add( physicsComponent.mPhysicsObject );
        },
        "AddStateComponent",
        [&]( Entity& entity, Any object ) { scene.AddComponent<StateComponent>( entity, object ); },

        // Get
        "GetTagComponent",
        [&]( Entity& entity ) ->TagComponent& { return scene.GetComponent<TagComponent>( entity ); },
        "GetTransformComponent",
        [&]( Entity& entity ) ->TransformComponent& { return scene.GetComponent<TransformComponent>( entity ); },
        "GetModelComponent",
        [&]( Entity& entity ) ->Ref<Model>& { return scene.GetComponent<ModelComponent>( entity ).mModel; },
        "GetShaderComponent",
        [&]( Entity& entity ) ->Ref<Shader>& { return scene.GetComponent<ShaderComponent>( entity ).mShader; },
        "GetPhysicsComponent",
        [&]( Entity& entity ) ->PhysicsObject& { return scene.GetComponent<PhysicsComponent>( entity ).mPhysicsObject; },
        "GetStateComponent",
        [&]( Entity& entity ) ->Any { return *scene.GetComponent<StateComponent>( entity ).mState; },

        // Has
        "HasTagComponent",
        [&]( Entity& entity ) ->bool { return scene.HasComponent<TagComponent>( entity ); },
        "HasTransformComponent",
        [&]( Entity& entity ) ->bool { return scene.HasComponent<TransformComponent>( entity ); },
        "HasModelComponent",
        [&]( Entity& entity ) ->bool { return scene.HasComponent<ModelComponent>( entity ); },
        "HasShaderComponent",
        [&]( Entity& entity ) ->bool { return scene.HasComponent<ShaderComponent>( entity ); },
        "HasPhysicsComponent",
        [&]( Entity& entity ) ->bool { return scene.HasComponent<PhysicsComponent>( entity ); },
        "HasStateComponent",
        [&]( Entity& entity ) ->bool { return scene.HasComponent<StateComponent>( entity ); }
    );


    // Scene
    lua["CreateEntity"] = [&](){ return scene.CreateEntity(); };

    lua["RemoveEntity"] = [&]( Entity entity ) { 
        if ( scene.HasComponent<PhysicsComponent>( entity ) )
            physicsEngine.Remove( scene.GetComponent<PhysicsComponent>( entity ).mPhysicsObject );
        scene.RemoveEntity( entity );
    };

    lua["ForEachEntity"] = [&]( sol::table components, sol::function func )
    {
        constexpr size_t componentsCount = magic_enum::enum_count<ComponentID>();
        using ComponentsIdsArray = std::array<ComponentTypeId, componentsCount>;
        using ComponentsDataArray = std::array<void*, componentsCount>;

        // Fill component ids
        ComponentsIdsArray componentsIds;
        componentsIds.fill( INVALID_COMPONENT_TYPE_ID );
        int i = 0;
        for ( auto [k, v] : components )
        {
            if ( not v.is<int>() )
                throw std::runtime_error( "RuntimeView expects array of components liKe Component.Tag" );
            componentsIds[i++] = (ComponentTypeId)v.as<int>();
        }


        // For each entity's components
        auto componentsAny = Any{};
        auto componentsTable = componentsAny.as<Table>();

        scene.RuntimeForEach( componentsIds,
        [&]( Entity entity, ComponentsDataArray componentsData )
        {
            for ( size_t i = 0; i < componentsCount; i++ )
            {
                if ( componentsIds[i] == INVALID_COMPONENT_TYPE_ID )
                    continue;

                switch ( (ComponentID)componentsIds[i] )
                {
                    case ComponentID::Tag:
                        componentsTable[ComponentID::Tag] = (TagComponent*)componentsData[i];
                        break;
                    case ComponentID::Transform:
                        componentsTable[ComponentID::Transform] = (TransformComponent*)componentsData[i];
                        break;
                    case ComponentID::Model:
                        componentsTable[ComponentID::Model] = *(Ref<Model>*)componentsData[i];
                        break;
                    case ComponentID::Shader:
                        componentsTable[ComponentID::Shader] = *(Ref<Shader>*)componentsData[i];
                        break;
                    case ComponentID::Script:
                        componentsTable[ComponentID::Script] = ((ScriptComponent*)componentsData[i])->mScript;
                        break;
                    case ComponentID::Physics:
                        componentsTable[ComponentID::Physics] = &((PhysicsComponent*)componentsData[i])->mPhysicsObject;
                        break;
                    case ComponentID::Camera:
                        // Camera component not handled
                        break;
                    case ComponentID::Light:
                        // Light component not handled
                        break;
                    case ComponentID::State:
                        componentsTable[ComponentID::State] = *( (StateComponent*)componentsData[i] )->mState;
                        break;
                    default:
                        throw std::runtime_error( "ForEachEntity: invalid set of componets provided" );
                }
            }
            func( entity, componentsAny );

        } );

    };

}

} // namespace bubble