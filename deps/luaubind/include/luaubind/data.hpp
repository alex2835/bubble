#pragma once
#include "luaubind/common.hpp"

struct lua_State;

// Script data handled where it lives, on the Luau stack - no C++ copy of a
// Luau value in between.
namespace luaubind
{
// The value at `index` as bytes: what save.write puts in a slot and how
// `progress` crosses into the next world. Data only - nil, booleans,
// numbers, strings, vectors, values of the engine's types that say how
// they save (LuaTypeBuilder::Saved), and tables of them. A function, other
// userdata or a thread, or a table inside itself, is an error with the path
// to it: "inventory[3]: a function cannot be saved". A table reached twice
// by different paths comes back as two tables.
expected<string, string> TryLuaEncode( lua_State* L, int index );

// Pushes the value TryLuaEncode turned into `bytes`; an engine type is
// found by its name in this state, which may not be the one that saved.
expected<void, string> TryLuaDecode( lua_State* L, string_view bytes );

// Pushes a copy of the value at `index` for snapshots of `self`: tables are
// copied all the way down, and a table reached twice is copied once, so
// sharing and cycles stay as they were. Functions, userdata and threads are
// the same values; metatables are shared.
expected<void, string> TryLuaDeepCopy( lua_State* L, int index );
}
