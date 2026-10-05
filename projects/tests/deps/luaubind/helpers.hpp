#pragma once
// What luaubind's tests share: compiling and running a snippet. Only
// luaubind and doctest - the library is tested apart from the engine.
#include <doctest.h>
#include <lua.h>
#include <luaubind/luaubind.hpp>
#include <ostream>

namespace luaubind::test
{
inline string Bytecode( string_view source )
{
    auto bytecode = CompileScript( source );
    REQUIRE_MESSAGE( bytecode.has_value(), ( bytecode ? "" : bytecode.error() ) );
    return *bytecode;
}

// Compiles and runs `source` in L's globals with the raw API; on success
// its results stay on the stack, else the error comes back.
inline expected<void, ScriptError> RunLua( lua_State* L, string_view source, int results = 0 )
{
    auto bytecode = CompileScript( source );
    if ( not bytecode )
        return std::unexpected( ScriptError{ bytecode.error(), {} } );
    if ( auto loaded = LoadScript( L, "=test", *bytecode ); not loaded )
        return std::unexpected( ScriptError{ loaded.error(), {} } );
    return PCall( L, 0, results );
}

// What `source` returns, run in the globals.
inline LuaValue Evaluate( LuaState& lua, string_view source )
{
    auto chunk = lua.Load( "=test", Bytecode( source ), lua.Globals() );
    REQUIRE_MESSAGE( chunk.has_value(), ( chunk ? "" : chunk.error() ) );
    auto value = ( *chunk )();
    REQUIRE_MESSAGE( value.has_value(), ( value ? "" : value.error().mMessage ) );
    return *value;
}

inline bool Balanced( const LuaState& lua )
{
    return lua_gettop( lua.L() ) == 0;
}
}
