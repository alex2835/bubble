#pragma once
#include "engine/reflection/reflection.hpp"
#include <sol/sol.hpp>

// A described type's fields as Lua sees them: every field of the description
// is a property of the usertype under the same name it has in a file and in
// a property.set path, read and set through engine/reflection - so a set runs
// OnChanged, and a ReadOnly field cannot be set.
//
// Values cross by value, as they always have: numbers, booleans and strings as
// themselves, vec2/3/4 as the glm usertypes, an enum by the name of its value
// ("spot"), a rotation (quat) as Euler radians in a vec3 - Lua has no
// quaternion - and described types and sequences as tables.
namespace bubble
{
sol::object ToLua( sol::state_view lua, const entt::meta_any& value );
// Throws saying what was expected when the value does not fit the type.
entt::meta_any FromLua( const sol::object& value, const entt::meta_type& type );

// Each enum of the described fields as a global table of its values:
// light_type.spot == "spot". Once per enum.
void BindReflectedEnum( sol::state& lua, const entt::meta_type& type );

// The usertype `name` for T, a property per described field. T's own
// constructors come from T::LuaConstructors (a sol::constructors<...>) when
// it has one, else the default constructor. The caller adds methods.
template <typename T>
sol::usertype<T> BindReflected( sol::state& lua, const char* name )
{
    sol::usertype<T> type = [&]
    {
        if constexpr ( requires { typename T::LuaConstructors; } )
            return lua.new_usertype<T>( name, sol::call_constructor, typename T::LuaConstructors() );
        else
            return lua.new_usertype<T>( name, sol::call_constructor, sol::constructors<T()>() );
    }();

    for ( const auto [id, field] : entt::resolve<T>().data() )
    {
        const char* fieldName = field.name();
        if ( field.type().is_enum() )
            BindReflectedEnum( lua, field.type() );

        auto get = [field]( const T& self, sol::this_state state )
        {
            const entt::meta_any object = entt::forward_as_meta( self );
            return ToLua( state, field.get( object ) );
        };
        if ( FieldInfoOf( field ).Has( FieldInfo::ReadOnly ) )
        {
            type.set( fieldName, sol::readonly_property( get ) );
            continue;
        }
        auto set = [field, fieldName]( T& self, const sol::object& value )
        {
            entt::meta_any object = entt::forward_as_meta( self );
            SetField( object, fieldName, FromLua( value, field.type() ) );
        };
        type.set( fieldName, sol::property( get, set ) );
    }
    return type;
}

}
