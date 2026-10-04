#include "bubble/scripts/lua/lua_data.hpp"
#include "bubble/types/containers.hpp"
#include "bubble/types/number.hpp"
#include <algorithm>
#include <bit>
#include <cstring>
#include <format>
#include <lua.h>
#include <lualib.h>

namespace bubble
{
namespace
{
// Every target - x64, ARM, wasm - stores numbers little-endian, so the
// bytes are the same everywhere.
static_assert( std::endian::native == std::endian::little );

// Bumped when the layout changes; old bytes are then refused, not converted.
constexpr u8 cFormat = 1;
constexpr int cMaxDepth = 100;

enum class Tag : u8
{
    Nil,
    False,
    True,
    Number,
    String,
    Vector,
    Table,
};

bool IsIdentifier( string_view text )
{
    if ( text.empty() or ( text[0] >= '0' and text[0] <= '9' ) )
        return false;
    return std::ranges::all_of( text, []( char c ) {
        return ( c >= 'a' and c <= 'z' ) or ( c >= 'A' and c <= 'Z' ) or ( c >= '0' and c <= '9' ) or c == '_';
    } );
}

// Where in the value the walk is: "inventory[3].name".
class Path
{
public:
    void PushKey( lua_State* L, int key )
    {
        switch ( lua_type( L, key ) )
        {
            case LUA_TSTRING:
            {
                const string_view name = lua_tostring( L, key );
                if ( IsIdentifier( name ) )
                    mSteps.push_back( mSteps.empty() ? string( name ) : "." + string( name ) );
                else
                    mSteps.push_back( std::format( "[\"{}\"]", name ) );
                break;
            }
            case LUA_TNUMBER: mSteps.push_back( std::format( "[{}]", lua_tonumber( L, key ) ) ); break;
            case LUA_TBOOLEAN: mSteps.push_back( lua_toboolean( L, key ) ? "[true]" : "[false]" ); break;
            default: mSteps.push_back( std::format( "[{}]", luaL_typename( L, key ) ) ); break;
        }
    }
    void Pop() { mSteps.pop_back(); }

    string Where() const
    {
        if ( mSteps.empty() )
            return "value";
        string where;
        for ( const string& step : mSteps )
            where += step;
        return where;
    }

private:
    vector<string> mSteps;
};

class Encoder
{
public:
    explicit Encoder( lua_State* L ) : mL( L ) { mOut.push_back( static_cast<char>( cFormat ) ); }

    expected<void, string> Value( int index, int depth )
    {
        lua_State* L = mL;
        index = lua_absindex( L, index );
        switch ( lua_type( L, index ) )
        {
            case LUA_TNIL: Put( Tag::Nil ); return {};
            case LUA_TBOOLEAN: Put( lua_toboolean( L, index ) ? Tag::True : Tag::False ); return {};
            case LUA_TNUMBER:
            {
                Put( Tag::Number );
                const double number = lua_tonumber( L, index );
                Put( &number, sizeof( number ) );
                return {};
            }
            case LUA_TSTRING:
            {
                size_t length = 0;
                const char* text = lua_tolstring( L, index, &length );
                Put( Tag::String );
                const auto size = static_cast<u32>( length );
                Put( &size, sizeof( size ) );
                Put( text, length );
                return {};
            }
            case LUA_TVECTOR:
                Put( Tag::Vector );
                Put( lua_tovector( L, index ), 3 * sizeof( float ) );
                return {};
            case LUA_TTABLE: return Table( index, depth );
            default:
                return std::unexpected(
                    std::format( "{}: a {} cannot be saved", mPath.Where(), luaL_typename( L, index ) ) );
        }
    }

    string Take() { return std::move( mOut ); }

private:
    expected<void, string> Table( int index, int depth )
    {
        lua_State* L = mL;
        if ( depth >= cMaxDepth )
            return std::unexpected( std::format( "{}: tables nested deeper than {}", mPath.Where(), cMaxDepth ) );
        const void* table = lua_topointer( L, index );
        if ( std::ranges::find( mOpen, table ) != mOpen.end() )
            return std::unexpected( std::format( "{}: the table contains itself", mPath.Where() ) );
        mOpen.push_back( table );
        luaL_checkstack( L, 4, "saving nested tables" );

        Put( Tag::Table );
        const size_t countAt = mOut.size();
        u32 count = 0;
        Put( &count, sizeof( count ) );

        lua_pushnil( L );
        while ( lua_next( L, index ) )
        {
            const int key = lua_gettop( L ) - 1;
            const int type = lua_type( L, key );
            if ( type != LUA_TSTRING and type != LUA_TNUMBER and type != LUA_TBOOLEAN )
                return std::unexpected(
                    std::format( "{}: a {} key cannot be saved", mPath.Where(), luaL_typename( L, key ) ) );
            if ( auto done = Value( key, depth + 1 ); not done )
                return done;
            mPath.PushKey( L, key );
            if ( auto done = Value( key + 1, depth + 1 ); not done )
                return done;
            mPath.Pop();
            lua_pop( L, 1 );
            ++count;
        }
        std::memcpy( mOut.data() + countAt, &count, sizeof( count ) );
        mOpen.pop_back();
        return {};
    }

    void Put( Tag tag ) { mOut.push_back( static_cast<char>( tag ) ); }
    void Put( const void* data, size_t size ) { mOut.append( static_cast<const char*>( data ), size ); }

    lua_State* mL;
    string mOut;
    Path mPath;
    // Tables on the way down to here: meeting one again is a cycle.
    vector<const void*> mOpen;
};

class Decoder
{
public:
    Decoder( lua_State* L, string_view in ) : mL( L ), mIn( in ) {}

    expected<void, string> Value( int depth )
    {
        lua_State* L = mL;
        u8 tag = 0;
        if ( not Get( &tag, 1 ) )
            return Short();
        switch ( static_cast<Tag>( tag ) )
        {
            case Tag::Nil: lua_pushnil( L ); return {};
            case Tag::False: lua_pushboolean( L, false ); return {};
            case Tag::True: lua_pushboolean( L, true ); return {};
            case Tag::Number:
            {
                double number = 0;
                if ( not Get( &number, sizeof( number ) ) )
                    return Short();
                lua_pushnumber( L, number );
                return {};
            }
            case Tag::String:
            {
                u32 size = 0;
                if ( not Get( &size, sizeof( size ) ) or mIn.size() - mAt < size )
                    return Short();
                lua_pushlstring( L, mIn.data() + mAt, size );
                mAt += size;
                return {};
            }
            case Tag::Vector:
            {
                float v[3] = {};
                if ( not Get( v, sizeof( v ) ) )
                    return Short();
                lua_pushvector( L, v[0], v[1], v[2] );
                return {};
            }
            case Tag::Table: return Table( depth );
        }
        return std::unexpected( "the data is damaged: unknown value"s );
    }

    bool AtEnd() const { return mAt == mIn.size(); }

    bool Get( void* out, size_t size )
    {
        if ( mIn.size() - mAt < size )
            return false;
        std::memcpy( out, mIn.data() + mAt, size );
        mAt += size;
        return true;
    }

private:
    expected<void, string> Table( int depth )
    {
        lua_State* L = mL;
        if ( depth >= cMaxDepth )
            return std::unexpected( "the data is damaged: nested too deep"s );
        u32 count = 0;
        if ( not Get( &count, sizeof( count ) ) )
            return Short();
        luaL_checkstack( L, 4, "loading nested tables" );
        lua_newtable( L );
        for ( u32 i = 0; i < count; ++i )
        {
            if ( auto done = Value( depth + 1 ); not done )
                return done;
            const bool nan = lua_type( L, -1 ) == LUA_TNUMBER and lua_tonumber( L, -1 ) != lua_tonumber( L, -1 );
            if ( lua_isnil( L, -1 ) or nan )
                return std::unexpected( "the data is damaged: a nil or NaN key"s );
            if ( auto done = Value( depth + 1 ); not done )
                return done;
            lua_rawset( L, -3 );
        }
        return {};
    }

    static expected<void, string> Short() { return std::unexpected( "the data is cut short"s ); }

    lua_State* mL;
    string_view mIn;
    size_t mAt = 0;
};

class Copier
{
public:
    Copier( lua_State* L, int memo ) : mL( L ), mMemo( memo ) {}

    expected<void, string> Copy( int index, int depth )
    {
        lua_State* L = mL;
        index = lua_absindex( L, index );
        if ( not lua_istable( L, index ) )
        {
            lua_pushvalue( L, index );
            return {};
        }
        // Copied already: the same copy, so sharing and cycles hold.
        lua_pushvalue( L, index );
        lua_rawget( L, mMemo );
        if ( not lua_isnil( L, -1 ) )
            return {};
        lua_pop( L, 1 );
        if ( depth >= cMaxDepth )
            return std::unexpected( std::format( "tables nested deeper than {}", cMaxDepth ) );
        luaL_checkstack( L, 6, "copying nested tables" );

        lua_createtable( L, lua_objlen( L, index ), 0 );
        const int copy = lua_gettop( L );
        lua_pushvalue( L, index );
        lua_pushvalue( L, copy );
        lua_rawset( L, mMemo );
        if ( lua_getmetatable( L, index ) )
            lua_setmetatable( L, copy );

        lua_pushnil( L );
        while ( lua_next( L, index ) )
        {
            const int key = lua_gettop( L ) - 1;
            if ( auto done = Copy( key, depth + 1 ); not done )
                return done;
            if ( auto done = Copy( key + 1, depth + 1 ); not done )
                return done;
            lua_rawset( L, copy );
            lua_pop( L, 1 );
        }
        return {};
    }

private:
    lua_State* mL;
    int mMemo;
};
}

expected<string, string> LuaEncode( lua_State* L, int index )
{
    index = lua_absindex( L, index );
    const int top = lua_gettop( L );
    Encoder encoder( L );
    auto done = encoder.Value( index, 0 );
    lua_settop( L, top );
    if ( not done )
        return std::unexpected( std::move( done.error() ) );
    return encoder.Take();
}

expected<void, string> LuaDecode( lua_State* L, string_view bytes )
{
    const int top = lua_gettop( L );
    Decoder decoder( L, bytes );
    u8 format = 0;
    if ( not decoder.Get( &format, 1 ) )
        return std::unexpected( "the data is empty"s );
    if ( format != cFormat )
        return std::unexpected( std::format( "the data is of format {}, this build reads {}", format, cFormat ) );
    auto done = decoder.Value( 0 );
    if ( done and not decoder.AtEnd() )
        done = std::unexpected( "the data is damaged: bytes after the value"s );
    if ( not done )
    {
        lua_settop( L, top );
        return done;
    }
    return {};
}

expected<void, string> LuaDeepCopy( lua_State* L, int index )
{
    index = lua_absindex( L, index );
    const int top = lua_gettop( L );
    lua_newtable( L );
    Copier copier( L, top + 1 );
    auto done = copier.Copy( index, 0 );
    if ( not done )
    {
        lua_settop( L, top );
        return done;
    }
    lua_remove( L, top + 1 );
    return {};
}
}
