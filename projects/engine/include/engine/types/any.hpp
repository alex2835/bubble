
#include <sol/forward.hpp>
#include "engine/types/json.hpp"

namespace bubble
{
class ScriptingEngine;

using Any = sol::lua_value;
using Table = sol::table;
using Object = sol::object;

bool IsClass( const Table& tbl );
bool IsArray( const Table& tbl );
void PrintAnyValue( const Any& value );

void DrawFieldsAdding( Table& table );
opt<Any> DrawAnyValue( const string& name, Any any );

json SaveAnyValue( const Any& v );
Any LoadAnyValue( ScriptingEngine& scripting, const json& j );

Any AnyDeepCopy( const Any& any );
Scope<Any> AnyDeepCopy( const Scope<Any>& any );
}