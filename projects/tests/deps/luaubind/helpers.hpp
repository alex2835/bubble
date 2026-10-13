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
    auto bytecodeMaybe = TryCompileScript( source );
    REQUIRE_MESSAGE( bytecodeMaybe.has_value(), ( bytecodeMaybe ? "" : bytecodeMaybe.error() ) );
    return *bytecodeMaybe;
}

// Compiles and runs `source` in L's globals with the raw API; on success
// its results stay on the stack, else the error comes back.
inline expected<void, ScriptError> TryRunLua( lua_State* L, string_view source, int results = 0 )
{
    auto bytecodeMaybe = TryCompileScript( source );
    if ( not bytecodeMaybe )
        return std::unexpected( ScriptError{ bytecodeMaybe.error(), {} } );
    if ( auto loadedMaybe = TryLoadScript( L, "=test", *bytecodeMaybe ); not loadedMaybe )
        return std::unexpected( ScriptError{ loadedMaybe.error(), {} } );
    return TryPCall( L, 0, results );
}

// What `source` returns, run in the globals.
inline LuaValue Evaluate( LuaState& lua, string_view source )
{
    auto chunkMaybe = lua.TryLoad( "=test", Bytecode( source ), lua.Globals() );
    REQUIRE_MESSAGE( chunkMaybe.has_value(), ( chunkMaybe ? "" : chunkMaybe.error() ) );
    auto valueMaybe = ( *chunkMaybe )();
    REQUIRE_MESSAGE( valueMaybe.has_value(), ( valueMaybe ? "" : valueMaybe.error().mMessage ) );
    return *valueMaybe;
}

inline bool Balanced( const LuaState& lua )
{
    return lua_gettop( lua.L() ) == 0;
}
}
