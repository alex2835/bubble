
#include "engine/types/dynamic.hpp"
#include "engine/types/string.hpp"
#include <sol/sol.hpp>
#include <print>

namespace bubble
{
bool IsObject( const sol::table& tbl )
{
    return tbl.valid() and tbl[sol::metatable_key] != sol::nil;
}

bool IsArray( const sol::table& tbl )
{
    int lastKey = 0;
    for ( auto& [key, value] : tbl )
    {
        // Found a non-integer key or not sequential 
        if ( not key.is<int>() or
             key.as<int>() - lastKey != 1 )
            return false;
    }
    return true;
}


void bubble::PrintAnyValue( const Any& value )
{
    if ( value.is<sol::nil_t>() )
        std::print( "nil" );
    else if ( value.is<sol::table>() )
    {
        const auto& table = value.as<sol::table>();
        if ( IsObject( table ) )
        {
            auto metatable = table[sol::metatable_key];
            sol::optional<sol::function> tostring_fn = metatable["__tostring"];
            if ( tostring_fn != sol::nil )
            {
                std::string className = metatable["__name"].get_or( std::string( "user type" ) );
                std::string objectString = ( *tostring_fn )( table );
                std::print( "({}){}", className, objectString );
            }
            else
                std::print( "(user class) no __tostring" );
        }
        else if ( IsArray( table ) ) // array
        {
            std::print( "[" );
            for ( auto& [idx, value] : table )
            {
                PrintAnyValue( value );
                std::print( ", " );
            }
            std::print( "]" );
        }
        else // map
        {
            std::print( "{{" );
            for ( auto& [key, value] : table )
            {
                PrintAnyValue( key );
                std::print( " : " );
                PrintAnyValue( value );
                std::print( "; " );
            }
            std::print( "}}" );
        }
    }
    else
    {
        if ( value.is<int>() )
            std::print( "(int)'{}'", value.as<int>() );
        else if ( value.is<float>() )
            std::print( "(float)'{}'", value.as<float>() );
        else if ( value.is<std::string>() )
            std::print( "(string)'{}'", value.as<std::string>() );
        else if ( value.is<bool>() )
            std::print( "(bool)'{}'", value.as<bool>() );
        else
            std::print( "(unknown)" );
    }
}

}
