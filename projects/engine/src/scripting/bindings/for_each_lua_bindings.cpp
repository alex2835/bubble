#include "engine/pch/pch.hpp"
#include "engine/scripting/bindings/for_each_lua_bindings.hpp"
#include "engine/scene/component_manager.hpp"
#include "engine/scene/scene.hpp"
#include "binding_utils.hpp"
#include <sol/sol.hpp>

namespace bubble
{

void CreateForEachBindings( Scene& scene, sol::state& lua )
{
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
                        componentsTable[ComponentLuaName<TagComponent>()] = (TagComponent*)componentDataPtr;
                        break;
                    case ComponentID::Transform:
                        componentsTable[ComponentLuaName<TransformComponent>()] = (TransformComponent*)componentDataPtr;
                        break;
                    case ComponentID::Model:
                        componentsTable[ComponentLuaName<ModelComponent>()] = (ModelComponent*)componentDataPtr;
                        break;
                    case ComponentID::Shader:
                        componentsTable[ComponentLuaName<ShaderComponent>()] = (ShaderComponent*)componentDataPtr;
                        break;
                    case ComponentID::Script:
                        componentsTable[ComponentLuaName<ScriptComponent>()] = (ScriptComponent*)componentDataPtr;
                        break;
                    case ComponentID::RigidBody:
                        componentsTable[ComponentLuaName<RigidBodyComponent>()] = (RigidBodyComponent*)componentDataPtr;
                        break;
                    case ComponentID::CharacterController:
                        componentsTable[ComponentLuaName<CharacterControllerComponent>()] = (CharacterControllerComponent*)componentDataPtr;
                        break;
                    case ComponentID::Camera:
                        componentsTable[ComponentLuaName<CameraComponent>()] = (CameraComponent*)componentDataPtr;
                        break;
                    case ComponentID::Light:
                        componentsTable[ComponentLuaName<LightComponent>()] = (LightComponent*)componentDataPtr;
                        break;
                    case ComponentID::State:
                        componentsTable[ComponentLuaName<StateComponent>()] = *((StateComponent*)componentDataPtr)->mState;
                        break;
                    default:
                        throw std::runtime_error( "for_each_entity: invalid set of components provided" );
                }
            }
            func( entity, componentsTable );
        } );
    };
}

}
