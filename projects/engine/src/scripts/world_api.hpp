#pragma once
// What a world's scripts see of the engine besides the runtime's own
// globals: the entity, color and asset types and their constructors for
// props; each component a type of its own, reached through its entity -
// self.entity.transform.position - with the fields its description gives;
// the services described as such, by name - debug_draw.line( ... ); the
// input's actions; spawn( prefab ) into the running world; and math.random
// drawing on the world's Random.
#include "bubble/scripts/script_api.hpp"

namespace bubble
{
class AssetRegistry;

// Into `lua` before it is sealed, with what the modules add (`api`);
// `world` and `assets` outlive it.
void RegisterWorldApi( LuaState& lua, World& world, AssetRegistry& assets, const ScriptApi& api );
}
