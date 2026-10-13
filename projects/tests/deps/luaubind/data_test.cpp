// Script data as bytes (saves, progress) and copied in place (snapshots).
#include "deps/luaubind/helpers.hpp"

using namespace luaubind;
using namespace luaubind::test;

namespace
{
// Leaves the value `expression` evaluates to on the stack.
void Push( lua_State* L, string_view expression )
{
    REQUIRE( TryRunLua( L, "return " + string( expression ), 1 ) );
}

// Runs `check` with the value on top of the stack, which it takes, as `value`.
bool Holds( lua_State* L, string_view check )
{
    auto bytecodeMaybe = TryCompileScript( "local value = ...; return " + string( check ) );
    REQUIRE( bytecodeMaybe );
    REQUIRE( TryLoadScript( L, "=check", *bytecodeMaybe ) );
    lua_insert( L, -2 );
    REQUIRE( TryPCall( L, 1, 1 ) );
    const bool result = lua_toboolean( L, -1 );
    lua_pop( L, 1 );
    return result;
}
}

TEST_CASE( "Data goes to bytes and comes back the same" )
{
    LuaState state;
    lua_State* L = state.L();
    Push( L, R"({
        name = "Странник 🌙", level = 12, alive = true, speed = 4.25,
        spawn = vector.create( 1, 2, 3 ),
        inventory = { "sword", "key", { kind = "potion", count = 3 } },
        [7] = false,
    })" );
    auto bytesMaybe = TryLuaEncode( L, -1 );
    REQUIRE_MESSAGE( bytesMaybe.has_value(), bytesMaybe.error() );
    lua_pop( L, 1 );

    REQUIRE( TryLuaDecode( L, *bytesMaybe ) );
    CHECK( Holds( L, R"(value.name == "Странник 🌙" and value.level == 12 and value.alive and value.speed == 4.25
        and value.spawn == vector.create( 1, 2, 3 ) and #value.inventory == 3
        and value.inventory[3].kind == "potion" and value.inventory[3].count == 3 and value[7] == false)" ) );
    CHECK( lua_gettop( L ) == 0 );
}

TEST_CASE( "What cannot be saved is named with its path" )
{
    LuaState state;
    lua_State* L = state.L();
    Push( L, "{ inventory = { 'sword', 'key', print } }" );
    auto bytesMaybe = TryLuaEncode( L, -1 );
    REQUIRE_FALSE( bytesMaybe.has_value() );
    CHECK( bytesMaybe.error() == "inventory[3]: a function cannot be saved" );
    CHECK( lua_gettop( L ) == 1 );
    lua_pop( L, 1 );

    Push( L, "(function() local t = { child = {} }; t.child.parent = t; return t end)()" );
    auto cycleMaybe = TryLuaEncode( L, -1 );
    REQUIRE_FALSE( cycleMaybe.has_value() );
    CHECK( cycleMaybe.error() == "child.parent: the table contains itself" );
    lua_pop( L, 1 );
}

TEST_CASE( "Damaged bytes are refused, not half-read" )
{
    LuaState state;
    lua_State* L = state.L();
    Push( L, "{ 1, 2, 3, name = 'x' }" );
    const string bytes = *TryLuaEncode( L, -1 );
    lua_pop( L, 1 );

    CHECK_FALSE( TryLuaDecode( L, "" ).has_value() );
    CHECK_FALSE( TryLuaDecode( L, string_view( bytes ).substr( 0, bytes.size() - 1 ) ).has_value() );
    CHECK_FALSE( TryLuaDecode( L, bytes + "x" ).has_value() );
    string otherFormat = bytes;
    otherFormat[0] = 99;
    CHECK( TryLuaDecode( L, otherFormat ).error().find( "format 99" ) != string::npos );
    CHECK( lua_gettop( L ) == 0 );
}

TEST_CASE( "A deep copy is independent but keeps sharing, cycles and functions" )
{
    LuaState state;
    lua_State* L = state.L();
    Push( L, R"((function()
        local shared = { hp = 10 }
        local t = { a = shared, b = shared, f = print, list = { 1, 2 } }
        t.self = t
        return setmetatable( t, { kind = "unit" } )
    end)())" );
    REQUIRE( TryLuaDeepCopy( L, -1 ) );
    // The original is under the copy.
    lua_pushvalue( L, -2 );
    lua_setglobal( L, "original" );
    CHECK( Holds( L, R"(value ~= original and value.a ~= original.a and value.a == value.b
        and value.self == value and value.f == print and getmetatable( value ) == getmetatable( original )
        and value.list[2] == 2)" ) );
    lua_pop( L, 1 );
    CHECK( lua_gettop( L ) == 0 );
}
