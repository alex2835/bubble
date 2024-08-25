#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>
#include <format>
#include "engine/scripting/bindings/scene_lua_bindings.hpp"
#include "engine/scene/components.hpp"

namespace bubble
{
void CreateSceneBindings( sol::state& lua )
{
    lua.new_usertype<Entity>(
        "Entity",
        "AddTagComponent", &Entity::AddComponet<TagComponent, string>
    );

    lua.new_usertype<Scene>(
        "Scene",
        "CreateEntity", &Scene::CreateEntity
    );
}

} // namespace bubble