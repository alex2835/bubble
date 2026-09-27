#pragma once
#include <sol/forward.hpp>
#include "engine/types/string.hpp"
#include "engine/types/pointer.hpp"

namespace bubble
{
// A Lua value held from C++: what a script's `state` table, a shader's uniform
// table and global_state are. Everything that turns one into something else
// lives next to that something - serialization/any_serialization.hpp for JSON,
// editing/ui/lua_table_widget.hpp for the inspector, renderer/shader_uniforms.hpp for the
// GPU block.
using Any = sol::lua_value;
using Table = sol::table;
using Object = sol::object;

bool IsClass( const Table& tbl );
bool IsArray( const Table& tbl );
string AnyValueToString( const Any& value );
void PrintAnyValue( const Any& value );

Any AnyDeepCopy( const Any& any );

// A Lua value as text for a person: tables opened up to `depth` levels,
// keys sorted, a table met twice shown as <cycle>, userdata by its
// __tostring. `multiline` lays a table that does not fit one line out one
// entry per line, indented; otherwise all on one line. What print and
// dump show, in the game's scripts and the editor's console alike.
string DescribeLuaValue( const sol::object& value, int depth = 3, bool multiline = true );
Scope<Any> AnyDeepCopy( const Scope<Any>& any );

}
