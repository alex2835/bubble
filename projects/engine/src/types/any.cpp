#include "engine/pch/pch.hpp"
#include "engine/types/any.hpp"
#include "engine/scene/scene.hpp"
#include "engine/renderer/texture.hpp"
#include <sol/sol.hpp>
#include <print>

namespace bubble
{
bool IsClass( const Table& tbl )
{
    if ( not tbl.valid() )
        return false;
    sol::object meta = tbl[sol::metatable_key];
    if ( meta == sol::nil )
        return false;
    return meta.as<sol::table>()["__name"] != sol::nil;
}

bool IsArray( const Table& tbl )
{
    if ( tbl.empty() )
        return false;

    for ( auto& [key, value] : tbl )
    {
        if ( not key.is<int>() or key.as<int>() < 1 )
            return false;
    }
    return true;
}

string AnyValueToString( const Any& value )
{
    if ( value.is<sol::nil_t>() )
        return "nil";
    else if ( value.is<Table>() )
    {
        const auto& table = value.as<Table>();
        if ( IsClass( table ) )
        {
            auto metatable = table[sol::metatable_key];
            sol::optional<sol::function> tostring_fn = metatable["__tostring"];
            if ( tostring_fn != sol::nil )
            {
                std::string className = metatable["__name"].get_or( std::string( "user type" ) );
                std::string objectString = ( *tostring_fn )( table );
                return std::format( "({}){}", className, objectString );
            }
            else
                return "(user class) no __tostring";
        }
        else if ( IsArray( table ) ) // array
        {
            string result = "[";
            for ( auto& [idx, val] : table )
                result += AnyValueToString( val ) + ", ";
            result += "]";
            return result;
        }
        else // map
        {
            string result = "{";
            for ( auto& [key, val] : table )
                result += AnyValueToString( key ) + " : " + AnyValueToString( val ) + "; ";
            result += "}";
            return result;
        }
    }
    else if ( value.is<int>() )
        return std::format( "(int)'{}'", value.as<int>() );
    else if ( value.is<float>() )
        return std::format( "(float)'{}'", value.as<float>() );
    else if ( value.is<std::string>() )
        return std::format( "(string)'{}'", value.as<std::string>() );
    else if ( value.is<bool>() )
        return std::format( "(bool)'{}'", value.as<bool>() );
    else if ( value.is<vec2>() )
    {
        const auto& v = value.as<vec2>();
        return std::format( "(vec2)'[{},{}]'", v.x, v.y );
    }
    else if ( value.is<vec3>() )
    {
        const auto& v = value.as<vec3>();
        return std::format( "(vec3)'[{},{},{}]'", v.x, v.y, v.z );
    }
    else if ( value.is<vec4>() )
    {
        const auto& v = value.as<vec4>();
        return std::format( "(vec4)'[{},{},{},{}]'", v.x, v.y, v.z, v.w );
    }
    else if ( value.is<mat3>() )
        return "(mat3)";
    else if ( value.is<mat4>() )
        return "(mat4)";
    else if ( value.is<Entity>() )
        return std::format( "(Entity)'{}'", (size_t)value.as<Entity>() );
    else if ( value.is<Ref<Texture2D>>() )
    {
        const auto& texture = value.as<Ref<Texture2D>>();
        return std::format( "(Texture2D)'{}'", texture ? texture->mPath.string() : "null" );
    }
    else
        return "(unknown)";
}

void PrintAnyValue( const Any& value )
{
    std::println( "{}", AnyValueToString( value ) );
}

Any AnyDeepCopy( const Any& any )
{
    if ( any.is<Table>() )
    {
        auto table = any.as<Table>();
        sol::state_view lua = table.lua_state();
        auto newTable = lua.create_table();
        for ( auto& [k, v] : table )
            newTable[k] = v.is<Table>() ? AnyDeepCopy( v ) : v;
        return newTable;
    }
    return any;
}

Scope<Any> AnyDeepCopy( const Scope<Any>& any )
{
    BUBBLE_ASSERT( any, "Empty pointer copy" );
    return CreateScope<Any>( AnyDeepCopy( *any ) );
}

} // namespace bubble
