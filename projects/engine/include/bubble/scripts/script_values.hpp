#pragma once
#include "bubble/data/value.hpp"
#include "bubble/scene/entity.hpp"
#include "bubble/scripts/lua.hpp"

namespace bubble
{
class Scene;

// A prop as a script sees it: what the scene's overrides become in self.
// Booleans, numbers, strings, vectors and tables as Luau has them - a text
// and a name are strings, a list an array, a record a table by field name -
// and a color, an asset and an entity as the engine's types, which the
// state must have: a world's scripts module registers them. A link to an
// entity resolves in `scene` from `from`; an empty one is the empty entity
// an `entity()` prop starts as. The error says where, from `where`.
expected<LuaValue, string> TryLuaFromValue( LuaState& lua,
                                            const Value& value,
                                            const Scene& scene,
                                            Entity from,
                                            string_view where );

// And back, as `type` says the value must be: what the inspector reads of
// self in Play. An entity becomes its path from the root; one that is gone,
// or the empty one, an empty path. Any takes the kind the value has.
expected<Value, string> TryValueFromLua( LuaState& lua,
                                         const LuaValue& value,
                                         const ValueType& type,
                                         const Scene& scene,
                                         string_view where );
}
