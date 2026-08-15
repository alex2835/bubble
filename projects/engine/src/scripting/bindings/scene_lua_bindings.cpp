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
    RigidBody = 7,
    CharacterController = 8,
    State = 9
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
        sol::overload(
            [&]( const Entity& entity, const string& tag ) { scene.AddComponent<TagComponent>( entity, tag ); },
            [&]( const Entity& entity, const TagComponent& c ) { scene.AddComponent<TagComponent>( entity, c ); }
        ),
        "AddTransformComponent",
        sol::overload(
            [&]( const Entity& entity, const Transform& t ) { scene.AddComponent<TransformComponent>( entity, t ); },
            [&]( const Entity& entity, const TransformComponent& c ) { scene.AddComponent<TransformComponent>( entity, c ); }
        ),
        "AddModelComponent",
        sol::overload(
            [&]( const Entity& entity, const Ref<Model>& model ) { scene.AddComponent<ModelComponent>( entity, model ); },
            [&]( const Entity& entity, const ModelComponent& c ) { scene.AddComponent<ModelComponent>( entity, c ); }
        ),
        "AddShaderComponent",
        sol::overload(
            [&]( const Entity& entity, const Ref<Shader>& shader ) { scene.AddComponent<ShaderComponent>( entity, shader ); },
            [&]( const Entity& entity, const ShaderComponent& c ) { scene.AddComponent<ShaderComponent>( entity, c ); }
        ),
        "AddCameraComponent",
        sol::overload(
            [&]( const Entity& entity, const Camera& camera ) { scene.AddComponent<CameraComponent>( entity, camera ); },
            [&]( const Entity& entity, const CameraComponent& c ) { scene.AddComponent<CameraComponent>( entity, c ); }
        ),
        "AddLightComponent",
        sol::overload(
            [&]( const Entity& entity, const Light& light ) { scene.AddComponent<LightComponent>( entity, light ); },
            [&]( const Entity& entity, const LightComponent& c ) { scene.AddComponent<LightComponent>( entity, c ); }
        ),
        "AddRigidBodyComponent",
        sol::overload(
            [&]( const Entity& entity, RigidBody object )
            {
                auto& c = scene.AddComponent<RigidBodyComponent>( entity, std::move( object ) );
                physicsEngine.Add( c.mRigidBody, entity );
            },
            [&]( const Entity& entity, RigidBodyComponent comp )
            {
                auto& c = scene.AddComponent<RigidBodyComponent>( entity, std::move( comp ) );
                physicsEngine.Add( c.mRigidBody, entity );
            }
        ),
        "AddCharacterControllerComponent",
        sol::overload(
            [&]( const Entity& entity, f32 radius, f32 height, f32 stepHeight )
            {
                auto& c = scene.AddComponent<CharacterControllerComponent>( entity, radius, height, stepHeight );
                physicsEngine.Add( c.mController, entity );
            },
            [&]( const Entity& entity, CharacterControllerComponent comp )
            {
                auto& c = scene.AddComponent<CharacterControllerComponent>( entity, std::move( comp ) );
                physicsEngine.Add( c.mController, entity );
            }
        ),
        "AddStateComponent",
        [&]( const Entity& entity, Any object ) { scene.AddComponent<StateComponent>( entity, object ); },

        // Get — inner/base type shortcuts (convenient field access)
        "GetTag",
        [&]( const Entity& entity ) -> TagComponent& { return scene.GetComponent<TagComponent>( entity ); },
        "GetTransform",
        [&]( const Entity& entity ) -> Transform& { return scene.GetComponent<TransformComponent>( entity ); },
        "GetModel",
        [&]( const Entity& entity ) -> Ref<Model> { return scene.GetComponent<ModelComponent>( entity ).mModel; },
        "GetShader",
        [&]( const Entity& entity ) -> Ref<Shader> { return scene.GetComponent<ShaderComponent>( entity ).mShader; },
        "GetCamera",
        [&]( const Entity& entity ) -> Camera& { return scene.GetComponent<CameraComponent>( entity ); },
        "GetLight",
        [&]( const Entity& entity ) -> Light& { return scene.GetComponent<LightComponent>( entity ); },
        "GetRigidBody",
        [&]( const Entity& entity ) -> RigidBody& { return scene.GetComponent<RigidBodyComponent>( entity ).mRigidBody; },
        "GetCharacterController",
        [&]( const Entity& entity ) -> CharacterController& { return scene.GetComponent<CharacterControllerComponent>( entity ).mController; },
        "GetState",
        [&]( const Entity& entity ) -> Any { return *scene.GetComponent<StateComponent>( entity ).mState; },

        // Get — full component wrapper (access component-level fields like mUniforms)
        "GetTagComponent",
        [&]( const Entity& entity ) -> TagComponent& { return scene.GetComponent<TagComponent>( entity ); },
        "GetTransformComponent",
        [&]( const Entity& entity ) -> TransformComponent& { return scene.GetComponent<TransformComponent>( entity ); },
        "GetModelComponent",
        [&]( const Entity& entity ) -> ModelComponent& { return scene.GetComponent<ModelComponent>( entity ); },
        "GetShaderComponent",
        [&]( const Entity& entity ) -> ShaderComponent& { return scene.GetComponent<ShaderComponent>( entity ); },
        "GetCameraComponent",
        [&]( const Entity& entity ) -> CameraComponent& { return scene.GetComponent<CameraComponent>( entity ); },
        "GetLightComponent",
        [&]( const Entity& entity ) -> LightComponent& { return scene.GetComponent<LightComponent>( entity ); },
        "GetRigidBodyComponent",
        [&]( const Entity& entity ) -> RigidBodyComponent& { return scene.GetComponent<RigidBodyComponent>( entity ); },
        "GetCharacterControllerComponent",
        [&]( const Entity& entity ) -> CharacterControllerComponent& { return scene.GetComponent<CharacterControllerComponent>( entity ); },
        "GetStateComponent",
        [&]( const Entity& entity ) -> Any { return *scene.GetComponent<StateComponent>( entity ).mState; },

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
        "HasRigidBodyComponent",
        [&]( const Entity& entity ) ->bool { return scene.HasComponent<RigidBodyComponent>( entity ); },
        "HasCharacterControllerComponent",
        [&]( const Entity& entity ) ->bool { return scene.HasComponent<CharacterControllerComponent>( entity ); },
        "HasStateComponent",
        [&]( const Entity& entity ) ->bool { return scene.HasComponent<StateComponent>( entity ); }
    );

    // Scene
    lua["CreateEntity"] = [&](){ return scene.CreateEntity(); };

    lua["RemoveEntity"] = [&]( Entity entity ) {
        if ( scene.HasComponent<RigidBodyComponent>( entity ) )
        {
            auto& rigidBody = scene.GetComponent<RigidBodyComponent>( entity );
            physicsEngine.Remove( rigidBody.mRigidBody );
        }
        if ( scene.HasComponent<CharacterControllerComponent>( entity ) )
        {
            auto& controller = scene.GetComponent<CharacterControllerComponent>( entity );
            physicsEngine.Remove( controller.mController );
        }
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
        auto componentsTable = lua.create_table();

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
                        throw std::runtime_error( "ForEachEntity: invalid set of components provided" );
                }
            }
            func( entity, componentsTable );
        } );
    };
}

} // namespace bubble