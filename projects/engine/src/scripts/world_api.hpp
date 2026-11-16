#pragma once
// What a world's scripts see of the engine besides the runtime's own
// globals: the entity, color and asset types and their constructors for
// props; each component a type of its own, reached through its entity -
// self.entity.transform.position - with the fields its description gives;
// the services described as such, by name - debug_draw.line( ... ); the
// input's actions; and math.random drawing on the world's Random.
#include "bubble/scripts/lua.hpp"

namespace bubble
{
class AssetRegistry;
class World;

// Into `lua` before it is sealed; `world` and `assets` outlive it.
void RegisterWorldApi( LuaState& lua, World& world, const AssetRegistry& assets );
}
