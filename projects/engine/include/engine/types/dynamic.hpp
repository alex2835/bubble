
#include <sol/forward.hpp>

namespace bubble
{
using Any = sol::lua_value;
using Table = sol::table;
using Object = sol::object;

bool IsClass( const Table& tbl );
bool IsArray( const Table& tbl );
void PrintAnyValue( const Any& value );
}