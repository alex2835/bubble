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
Scope<Any> AnyDeepCopy( const Scope<Any>& any );

}
