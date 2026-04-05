
#include <sol/forward.hpp>
#include "engine/types/json.hpp"

namespace bubble
{
class Project;

using Any = sol::lua_value;
using Table = sol::table;
using Object = sol::object;

bool IsClass( const Table& tbl );
bool IsArray( const Table& tbl );
string AnyValueToString( const Any& value );
void PrintAnyValue( const Any& value );

json SaveAnyValue( const Project& project, const Any& v );
Any LoadAnyValue( Project& project, const json& j );

Any AnyDeepCopy( const Any& any );
Scope<Any> AnyDeepCopy( const Scope<Any>& any );

void DrawFieldsAdding( Project& project, Table& table );
Any DrawAnyValue( Project& project, string_view name, Any any );

}