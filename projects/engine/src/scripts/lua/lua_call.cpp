#include "bubble/scripts/lua/lua_call.hpp"
#include "bubble/scripts/lua/lua_state.hpp"
#include <cstdlib>
#include <format>
#include <lua.h>
#include <luacode.h>
#include <lualib.h>
#include <memory>

namespace bubble
{
struct LuaStateAccess
{
    static string& LastTraceback( lua_State* L ) { return LuaState::Of( L ).mLastTraceback; }
};

namespace
{
// Runs where the error was raised, before the stack unwinds: the only
// moment the traceback can be taken. The message passes through as it is.
int ErrorHandler( lua_State* L )
{
    LuaStateAccess::LastTraceback( L ) = lua_debugtrace( L );
    return 1;
}
}

expected<string, string> CompileScript( string_view source, const CompileOptions& options )
{
    lua_CompileOptions compile = {};
    compile.optimizationLevel = options.mOptimization;
    // Line numbers and function names: enough for tracebacks.
    compile.debugLevel = 1;
    // Lets native code generation specialise on argument types everywhere.
    compile.typeInfoLevel = 1;

    size_t size = 0;
    const std::unique_ptr<char, decltype( &std::free )> bytecode(
        luau_compile( source.data(), source.size(), &compile, &size ), &std::free );
    // A failed compilation is bytecode that starts with 0 and then the error.
    if ( size == 0 or bytecode.get()[0] == 0 )
        return std::unexpected( size > 1 ? string( bytecode.get() + 1, size - 1 ) : "compilation failed"s );
    return string( bytecode.get(), size );
}

expected<void, string> LoadScript( lua_State* L, string_view chunk, string_view bytecode, int env )
{
    // "@" makes Luau print the name as it is, where a plain name would show
    // as [string "player.luau"].
    const bool marked = chunk.starts_with( '=' ) or chunk.starts_with( '@' );
    const string name = marked ? string( chunk ) : "@" + string( chunk );
    if ( luau_load( L, name.c_str(), bytecode.data(), bytecode.size(), env ) != 0 )
    {
        string error = lua_tostring( L, -1 );
        lua_pop( L, 1 );
        return std::unexpected( std::move( error ) );
    }
    return {};
}

expected<void, ScriptError> PCall( lua_State* L, int nargs, int nresults )
{
    const int function = lua_gettop( L ) - nargs;
    lua_pushcfunction( L, ErrorHandler, "error_handler" );
    lua_insert( L, function );
    string& traceback = LuaStateAccess::LastTraceback( L );
    traceback.clear();

    const int status = lua_pcall( L, nargs, nresults, function );
    if ( status == LUA_OK )
    {
        lua_remove( L, function );
        return {};
    }

    ScriptError error;
    if ( lua_type( L, -1 ) == LUA_TSTRING )
        error.mMessage = lua_tostring( L, -1 );
    else
        error.mMessage = "error object is " + DescribeValue( L, -1 );
    error.mTraceback = std::move( traceback );
    lua_pop( L, 2 );
    return std::unexpected( std::move( error ) );
}

string DescribeValue( lua_State* L, int index )
{
    switch ( lua_type( L, index ) )
    {
        case LUA_TNONE:
        case LUA_TNIL: return "nil";
        case LUA_TBOOLEAN: return lua_toboolean( L, index ) ? "true" : "false";
        case LUA_TNUMBER: return std::format( "{}", lua_tonumber( L, index ) );
        case LUA_TSTRING: return std::format( "\"{}\"", lua_tostring( L, index ) );
        case LUA_TVECTOR:
        {
            const float* v = lua_tovector( L, index );
            return std::format( "vector({}, {}, {})", v[0], v[1], v[2] );
        }
        default: return luaL_typename( L, index );
    }
}
}
