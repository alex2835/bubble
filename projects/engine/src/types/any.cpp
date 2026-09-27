#include "engine/pch/pch.hpp"
#include "engine/types/any.hpp"
#include <set>
#include <algorithm>
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

// By the value's Lua type, not is<Table>(): sol answers that yes for a
// userdata too - an Entity, a vec3 - and those came out of the copy as
// empty tables.
Any AnyDeepCopy( const Any& any )
{
    if ( any.value().get_type() == sol::type::table )
    {
        auto table = any.as<Table>();
        sol::state_view lua = table.lua_state();
        auto newTable = lua.create_table();
        for ( auto& [k, v] : table )
            newTable[k] = v.get_type() == sol::type::table ? AnyDeepCopy( v ) : v;
        return newTable;
    }
    return any;
}

Scope<Any> AnyDeepCopy( const Scope<Any>& any )
{
    BUBBLE_ASSERT( any, "Empty pointer copy" );
    return CreateScope<Any>( AnyDeepCopy( *any ) );
}

namespace
{
string DescribeNumber( const sol::object& value )
{
    lua_State* L = value.lua_state();
    value.push( L );
    const bool integer = lua_isinteger( L, -1 );
    const lua_Integer i = integer ? lua_tointeger( L, -1 ) : 0;
    const lua_Number n = lua_tonumber( L, -1 );
    lua_pop( L, 1 );
    if ( integer )
        return std::to_string( i );
    string s = std::format( "{:.6f}", n );
    // 1.500000 -> 1.5, 2.000000 -> 2.0
    s.erase( s.find_last_not_of( '0' ) + 1 );
    if ( s.back() == '.' )
        s += '0';
    return s;
}

bool IsIdentifier( const string& s )
{
    if ( s.empty() or std::isdigit( (unsigned char)s[0] ) )
        return false;
    return std::ranges::all_of( s, []( char c ) { return std::isalnum( (unsigned char)c ) or c == '_'; } );
}

string Describe( const sol::object& value, int depth, bool multiline, int indent, std::set<const void*>& open, bool quoteStrings )
{
    switch ( value.get_type() )
    {
        case sol::type::lua_nil:
        case sol::type::none:
            return "nil";
        case sol::type::boolean:
            return value.as<bool>() ? "true" : "false";
        case sol::type::number:
            return DescribeNumber( value );
        case sol::type::string:
            return quoteStrings ? std::format( "\"{}\"", value.as<string>() ) : value.as<string>();
        case sol::type::table:
            break;
        default:
        {
            // userdata (vec3, ...), functions: what tostring says.
            sol::state_view lua( value.lua_state() );
            return lua["tostring"]( value ).get<string>();
        }
    }

    const sol::table table = value.as<sol::table>();
    const void* self = table.pointer();
    if ( open.contains( self ) )
        return "<cycle>";

    // An array when the keys are 1..n; otherwise keys sorted, strings after
    // numbers, so the same table always reads the same.
    vector<std::pair<sol::object, sol::object>> entries;
    for ( const auto& [k, v] : table )
        entries.emplace_back( k, v );
    if ( entries.empty() )
        return "{}";
    if ( depth <= 0 )
        return std::format( "{{ ...{} }}", entries.size() );

    bool isArray = true;
    for ( const auto& [k, _] : entries )
        if ( k.get_type() != sol::type::number )
            isArray = false;
    std::ranges::sort( entries, [&]( const auto& a, const auto& b )
    {
        const bool an = a.first.get_type() == sol::type::number, bn = b.first.get_type() == sol::type::number;
        if ( an != bn )
            return an;
        if ( an )
            return a.first.template as<double>() < b.first.template as<double>();
        return Describe( a.first, 0, false, 0, open, false ) < Describe( b.first, 0, false, 0, open, false );
    } );
    if ( isArray )
        for ( size_t i = 0; i < entries.size(); i++ )
            if ( entries[i].first.as<double>() != double( i + 1 ) )
                isArray = false;

    open.insert( self );
    vector<string> parts;
    bool nested = false;
    for ( const auto& [k, v] : entries )
    {
        nested = nested or v.get_type() == sol::type::table;
        const string item = Describe( v, depth - 1, multiline, indent + 1, open, true );
        if ( isArray )
            parts.push_back( item );
        else if ( k.get_type() == sol::type::string and IsIdentifier( k.as<string>() ) )
            parts.push_back( std::format( "{} = {}", k.as<string>(), item ) );
        else
            parts.push_back( std::format( "[{}] = {}", Describe( k, 0, false, 0, open, true ), item ) );
    }
    open.erase( self );

    size_t width = 4;
    for ( const auto& p : parts )
        width += p.size() + 2;
    const bool fitsOneLine = width <= 80 and not ( nested and parts.size() > 1 );
    if ( not multiline or fitsOneLine )
    {
        string s = "{ ";
        for ( size_t i = 0; i < parts.size(); i++ )
            s += ( i ? ", " : "" ) + parts[i];
        return s + " }";
    }
    const string pad( 2 * ( indent + 1 ), ' ' );
    string s = "{\n";
    for ( const auto& p : parts )
        s += pad + p + ",\n";
    return s + string( 2 * indent, ' ' ) + "}";
}
}

string DescribeLuaValue( const sol::object& value, int depth, bool multiline )
{
    std::set<const void*> open;
    return Describe( value, depth, multiline, 0, open, true );
}

} // namespace bubble
