#pragma once
#include "bubble/data/value.hpp"
#include "bubble/scripts/lua.hpp"

// Values across the editor's Luau and Value: an operator's arguments in, a
// field's value out. The game's scripts get their own crossing at stage 7,
// with constructors for what Luau has no word for; here a string stands for
// a name, an entity or an asset, and an array of numbers for a color.
namespace bubble
{
// `value` as `type` declares it; Any takes the kind from the value: a
// boolean, a number, a string as text, a vector as vec3, a table with keys
// 1..n (or none) as a list and one with string keys as a record. The error
// says where: "entity.rename.name: text expected, got 3".
expected<Value, string> TryValueFromLua( LuaState& lua,
                                         const LuaValue& value,
                                         const ValueType& type,
                                         string_view where );

// A name, an entity and an asset as strings, a color as { r, g, b, a }.
LuaValue LuaFromValue( LuaState& lua, const Value& value );
}
