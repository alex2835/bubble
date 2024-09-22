#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>
#include <format>
#include <print>
#include "engine/scripting/bindings/scene_lua_bindings.hpp"
#include "engine/scene/components_manager.hpp"
#include "engine/utils/filesystem.hpp"

namespace bubble
{
void CreateSceneBindings( sol::state& lua )
{
    for ( const auto& [name, commpFuncTable] : ComponentManager::Instance() )
        commpFuncTable.mCreateLuaBinding( lua );

    lua.new_usertype<Entity>(
        "Entity",
        
        // Add
        "AddTagComponent",
        &Entity::AddComponent<TagComponent, string>,
        "AddTransformComponent", 
        []( Entity& entity, TransformComponent trasform ) { entity.AddComponent<TransformComponent>( trasform ); },
        "AddModelComponent",
        []( Entity& entity, Ref<Model> model ){ entity.AddComponent<ModelComponent>( model ); },
        "AddShaderComponent",
        []( Entity& entity, Ref<Shader> shader ) { entity.AddComponent<ShaderComponent>( shader ); },
        
        // Get
        "GetTagComponent",
        []( Entity& entity )->TagComponent& { return entity.GetComponent<TagComponent>(); },
        "GetTransformComponent",
        []( Entity& entity )->TransformComponent& { return entity.GetComponent<TransformComponent>(); },
        "GetModelComponent",
        []( Entity& entity )->Ref<Model>& { return entity.GetComponent<ModelComponent>(); },
        "GetShaderComponent",
        []( Entity& entity )->Ref<Shader>& { return entity.GetComponent<ShaderComponent>(); },

        // Has
        "HasTagComponent",
        &Entity::HasComponent<TagComponent>,
        "HasTransformComponent",
        &Entity::HasComponent<TransformComponent>,
        "HasModelComponent",
        &Entity::HasComponent<ModelComponent>,
        "HasShaderComponent",
        &Entity::HasComponent<ShaderComponent>
    );

    lua.new_usertype<Scene>(
        "Scene",
        "CreateEntity",
        &Scene::CreateEntity,
        "GetRuntimeView",
        []( Scene& scene, const sol::table& table )
        {
            return scene.GetRuntimeView( table.as<vector<string_view>>() );
        }
    );
}

} // namespace bubble