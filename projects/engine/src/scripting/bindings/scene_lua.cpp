#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>
#include <format>
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
        "AddTagComponent",
        &Entity::AddComponet<TagComponent, string>,
        "AddTransformComponent", 
        []( Entity& entity, TransformComponent trasform ) { entity.AddComponet<TransformComponent>( trasform ); },
        "AddModelComponent",
        []( Entity& entity, Ref<Model> model ){ entity.AddComponet<ModelComponent>( model ); },
        "AddShaderComponent",
        []( Entity& entity, Ref<Shader> shader ) { entity.AddComponet<ShaderComponent>( shader ); }
    );

    lua.new_usertype<Scene>(
        "Scene",
        "CreateEntity", &Scene::CreateEntity
    );
}

} // namespace bubble