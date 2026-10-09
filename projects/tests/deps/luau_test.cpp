// Luau builds and runs here - on every platform CI builds, the web too. The
// scripting module's tests grow from this.
#include <cstdlib>
#include <doctest.h>
#include <lua.h>
#include <luacode.h>
#include <lualib.h>
#include <string>

namespace
{
// Compiles and runs `source`; empty on success, else the error.
std::string Run( lua_State* L, const std::string& source )
{
    lua_CompileOptions options = {};
    options.optimizationLevel = 2;
    size_t size = 0;
    char* bytecode = luau_compile( source.data(), source.size(), &options, &size );
    const int loaded = luau_load( L, "=test", bytecode, size, 0 );
    std::free( bytecode );
    if ( loaded != 0 or lua_pcall( L, 0, 1, 0 ) != 0 )
    {
        std::string error = lua_tostring( L, -1 );
        lua_pop( L, 1 );
        return error;
    }
    return {};
}

struct LuauState
{
    lua_State* L = luaL_newstate();
    LuauState() { luaL_openlibs( L ); }
    ~LuauState() { lua_close( L ); }
};
}

TEST_CASE( "Luau runs a script with its vector type" )
{
    LuauState state;
    REQUIRE( Run( state.L, R"(
        local p = vector.create(1, 2, 3) + vector.create(0, 0.5, 0) * 2
        return p
    )" )
                 .empty() );
    const float* p = lua_tovector( state.L, -1 );
    REQUIRE( p != nullptr );
    CHECK( p[0] == 1.0f );
    CHECK( p[1] == 3.0f );
    CHECK( p[2] == 3.0f );
    lua_pop( state.L, 1 );
    CHECK( lua_gettop( state.L ) == 0 );
}

TEST_CASE( "A Luau error comes back as a message, not a crash" )
{
    LuauState state;
    const std::string error = Run( state.L, "local t = nil; return t.x" );
    CHECK( error.find( "attempt to index nil" ) != std::string::npos );
    // Compile errors too.
    CHECK_FALSE( Run( state.L, "return (" ).empty() );
}
