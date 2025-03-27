#include <sol/sol.hpp>
#include "engine/scripting/bindings/scene_lua_bindings.hpp"
#include "engine/scene/component_manager.hpp"
#include "engine/scene/scene.hpp"
#include "engine/utils/filesystem.hpp"

namespace bubble
{
void CreateSceneBindings( Scene& scene, sol::state& lua )
{
    for ( const auto& [name, commpFuncTable] : ComponentManager::Instance() )
        commpFuncTable.mCreateLuaBinding( lua );

    lua.new_usertype<Entity>(
        "Entity",
        
        // Add
        "AddTagComponent",
        [&]( Entity& entity, string tag ) { scene.AddComponent<TagComponent>( entity, tag ); },
        "AddTransformComponent", 
        [&]( Entity& entity, TransformComponent trasform ) { scene.AddComponent<TransformComponent>( entity, trasform ); },
        "AddModelComponent",
        [&]( Entity& entity, Ref<Model> model ) { scene.AddComponent<ModelComponent>( entity, model ); },
        "AddShaderComponent",
        [&]( Entity& entity, Ref<Shader> shader ) { scene.AddComponent<ShaderComponent>( entity, shader ); },
        
        // Get
        "GetTagComponent",
        [&]( Entity& entity ) ->TagComponent& { return scene.GetComponent<TagComponent>( entity ); },
        "GetTransformComponent",
        [&]( Entity& entity ) ->TransformComponent& { return scene.GetComponent<TransformComponent>( entity ); },
        "GetModelComponent",
        [&]( Entity& entity ) ->Ref<Model>& { return scene.GetComponent<ModelComponent>( entity ); },
        "GetShaderComponent",
        [&]( Entity& entity ) ->Ref<Shader>& { return scene.GetComponent<ShaderComponent>( entity ); },

        // Has
        "HasTagComponent",
        [&]( Entity& entity ) ->bool { return scene.HasComponent<TagComponent>( entity ); },
        "HasTransformComponent",
        [&]( Entity& entity ) ->bool { return scene.HasComponent<TransformComponent>( entity ); },
        "HasModelComponent",
        [&]( Entity& entity ) ->bool { return scene.HasComponent<ModelComponent>( entity ); },
        "HasShaderComponent",
        [&]( Entity& entity ) ->bool { return scene.HasComponent<ShaderComponent>( entity ); }
    );

    lua.new_usertype<Scene>(
        "Scene",
        "CreateEntity",
        &Scene::CreateEntity
        //"GetRuntimeView",
        //[]( Scene& scene, const sol::table& table )
        //{
        //    return scene.GetRuntimeView( table.as<vector<string_view>>() );
        //}
    );
}

} // namespace bubble