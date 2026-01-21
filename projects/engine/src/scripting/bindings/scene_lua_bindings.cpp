#include "engine/pch/pch.hpp"
#include "engine/scripting/bindings/scene_lua_bindings.hpp"
#include "engine/scene/component_manager.hpp"
#include "engine/scene/scene.hpp"
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
        [&]( const Entity& entity, const string& tag ) { scene.AddComponent<TagComponent>( entity, tag ); },
        "AddTransformComponent", 
        [&]( const Entity& entity, TransformComponent transform ) { scene.AddComponent<TransformComponent>( entity, transform ); },
        "AddModelComponent",
        [&]( const Entity& entity, const Ref<Model>& model ) { scene.AddComponent<ModelComponent>( entity, model ); },
        "AddShaderComponent",
        [&]( const Entity& entity, const Ref<Shader>& shader ) { scene.AddComponent<ShaderComponent>( entity, shader ); },
        "AddCameraComponent",
        [&]( const Entity& entity, const Camera& camera ) { scene.AddComponent<CameraComponent>( entity, camera ); },
        "AddLightComponent",
        [&]( const Entity& entity, const Light& light ) { scene.AddComponent<LightComponent>( entity, light ); },
        "AddPhysicsComponent",
        [&]( const Entity& entity, const PhysicsObject& object )
        {
            auto& physicsComponent = scene.AddComponent<PhysicsComponent>( entity, object );
            physicsEngine.Add( physicsComponent.mPhysicsObject, entity );
        },
        "AddStateComponent",
        [&]( const Entity& entity, Any object ) { scene.AddComponent<StateComponent>( entity, object ); },

        // Get
        "GetTagComponent",
        [&]( const Entity& entity ) ->TagComponent& { return scene.GetComponent<TagComponent>( entity ); },
        "GetTransformComponent",
        [&]( const Entity& entity ) ->TransformComponent& { return scene.GetComponent<TransformComponent>( entity ); },
        "GetModelComponent",
        [&]( const Entity& entity ) ->Ref<Model> { return scene.GetComponent<ModelComponent>( entity ).mModel; },
        "GetShaderComponent",
        [&]( const Entity& entity ) ->Ref<Shader> { return scene.GetComponent<ShaderComponent>( entity ).mShader; },
        "GetCameraComponent",
        [&]( const Entity& entity ) ->Camera& { return *(Camera*)&scene.GetComponent<CameraComponent>( entity ); },
        "GetLightComponent",
        [&]( const Entity& entity ) ->Light& { return *(Light*)&scene.GetComponent<LightComponent>( entity ); },
        "GetPhysicsComponent",
        [&]( const Entity& entity ) ->PhysicsObject& { return scene.GetComponent<PhysicsComponent>( entity ).mPhysicsObject; },
        "GetStateComponent",
        [&]( const Entity& entity ) ->Any { return *scene.GetComponent<StateComponent>( entity ).mState; },

        // Has
        "HasTagComponent",
        [&]( const Entity& entity ) ->bool { return scene.HasComponent<TagComponent>( entity ); },
        "HasTransformComponent",
        [&]( const Entity& entity ) ->bool { return scene.HasComponent<TransformComponent>( entity ); },
        "HasModelComponent",
        [&]( const Entity& entity ) ->bool { return scene.HasComponent<ModelComponent>( entity ); },
        "HasShaderComponent",
        [&]( const Entity& entity ) ->bool { return scene.HasComponent<ShaderComponent>( entity ); },
        "HasCameraComponent",
        [&]( const Entity& entity ) ->bool { return scene.HasComponent<CameraComponent>( entity ); },
        "HasLightComponent",
        [&]( const Entity& entity ) ->bool { return scene.HasComponent<LightComponent>( entity ); },
        "HasPhysicsComponent",
        [&]( const Entity& entity ) ->bool { return scene.HasComponent<PhysicsComponent>( entity ); },
        "HasStateComponent",
        [&]( const Entity& entity ) ->bool { return scene.HasComponent<StateComponent>( entity ); }
    );

    // Scene
    lua["CreateEntity"] = [&](){ return scene.CreateEntity(); };

    lua["RemoveEntity"] = [&]( Entity entity ) { 
        if ( scene.HasComponent<PhysicsComponent>( entity ) )
            physicsEngine.Remove( scene.GetComponent<PhysicsComponent>( entity ).mPhysicsObject );
        scene.RemoveEntity( entity );
    };

    lua["ForEachEntity"] = [&]( const sol::table& components, const sol::function& func )
    {
        constexpr size_t componentsCount = magic_enum::enum_count<ComponentID>();
        using ComponentsIdsArray = std::array<ComponentTypeId, componentsCount>;
        using ComponentsDataArray = std::array<void*, componentsCount>;

        // Fill component ids
        ComponentsIdsArray componentsIds;
        componentsIds.fill( INVALID_COMPONENT_TYPE_ID );
        int i = 0;
        for ( const auto& [k, v] : components )
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
                        componentsTable[ComponentID::Model] = *(Ref<Model>*)componentDataPtr;
                        break;
                    case ComponentID::Shader:
                        componentsTable[ComponentID::Shader] = *(Ref<Shader>*)componentDataPtr;
                        break;
                    case ComponentID::Script:
                        componentsTable[ComponentID::Script] = ((ScriptComponent*)componentDataPtr)->mScript;
                        break;
                    case ComponentID::Physics:
                        componentsTable[ComponentID::Physics] = &((PhysicsComponent*)componentDataPtr)->mPhysicsObject;
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
                        throw std::runtime_error( "ForEachEntity: invalid set of componets provided" );
                }
            }
            func( entity, componentsAny );
        } );
    };
}

} // namespace bubble