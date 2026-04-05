#include "engine/scripting/bindings/free_function_lua_bindings.hpp"
#include "engine/types/any.hpp"
#include <sol/sol.hpp>

namespace bubble
{
void CreateFreeFunctionBindings( sol::state& lua )
{
    lua["PrintAny"] = []( const Any& value ) { PrintAnyValue( value ) ; };
}

} // namespace bubble
