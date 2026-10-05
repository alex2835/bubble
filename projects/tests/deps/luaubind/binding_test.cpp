// luaubind's binding layer: holding values, converting them, binding
// functions and types, calling with tracebacks.
#include "deps/luaubind/helpers.hpp"
#include <memory>
#include <stdexcept>
#include <tuple>

using namespace luaubind;
using namespace luaubind::test;

namespace
{
struct Vec3
{
    f32 x = 0, y = 0, z = 0;
};

string ErrorOf( lua_State* L, string_view source )
{
    auto ran = RunLua( L, source );
    REQUIRE_FALSE( ran.has_value() );
    return ran.error().mMessage;
}
}

TEST_CASE( "A LuaValue keeps its value through a collection; a copy is the same value" )
{
    LuaState state;
    LuaValue table = state.NewTable();
    state.RawSet( table, "answer", 42 );
    lua_gc( state.L(), LUA_GCCOLLECT, 0 );

    LuaValue copy = table;
    table = {};
    CHECK( table.IsNil() );
    CHECK( state.RawGet( copy, "answer" ).As<int>() == 42 );
    // The same table, not a second one.
    LuaValue again = copy;
    state.RawSet( again, "answer", 7 );
    CHECK( state.RawGet( copy, "answer" ).As<int>() == 7 );
    CHECK( again == copy );
    CHECK_FALSE( again == state.NewTable() );
    CHECK( lua_gettop( state.L() ) == 0 );
}

TEST_CASE( "Values cross both ways with their types" )
{
    LuaState state;
    lua_State* L = state.L();
    LuaPush( L, true );
    LuaPush( L, 7 );
    LuaPush( L, 2.5 );
    LuaPush( L, string( "привет" ) );
    LuaPush( L, Vec3{ 1, 2, 3 } );
    LuaPush( L, opt<int>() );
    CHECK( LuaCheck<bool>( L, 1 ) );
    CHECK( LuaCheck<int>( L, 2 ) == 7 );
    CHECK( LuaCheck<f64>( L, 3 ) == 2.5 );
    CHECK( LuaCheck<string>( L, 4 ) == "привет" );
    const Vec3 v = LuaCheck<Vec3>( L, 5 );
    CHECK( ( v.x == 1 and v.y == 2 and v.z == 3 ) );
    CHECK_FALSE( LuaCheck<opt<int>>( L, 6 ).has_value() );
    CHECK( LuaCheck<opt<int>>( L, 2 ) == 7 );
    lua_settop( L, 0 );
}

TEST_CASE( "A bound function checks its arguments and says which one is wrong" )
{
    LuaState state;
    lua_State* L = state.L();
    LuaPushFunction( L, "scale", []( f32 value, int times ) { return value * static_cast<f32>( times ); } );
    lua_setglobal( L, "scale" );

    REQUIRE( RunLua( L, "return scale( 1.5, 4 )", 1 ) );
    CHECK( lua_tonumber( L, -1 ) == 6 );
    lua_pop( L, 1 );

    const string wrongType = ErrorOf( L, "return scale( 'big', 2 )" );
    CHECK( wrongType.find( "#1" ) != string::npos );
    CHECK( wrongType.find( "number expected, got string" ) != string::npos );
    CHECK( ErrorOf( L, "return scale( 1, 2.5 )" ).find( "integer" ) != string::npos );
}

TEST_CASE( "A bound function keeps its captures, takes the state and returns several values" )
{
    LuaState state;
    lua_State* L = state.L();
    int calls = 0;
    LuaPushFunction( L, "split", [&calls]( lua_State* from, Vec3 v ) {
        ++calls;
        CHECK( from != nullptr );
        return std::tuple( v.x, v.y, v.z );
    } );
    lua_setglobal( L, "split" );

    REQUIRE( RunLua( L, "return split( vector.create( 1, 2, 3 ) )", 3 ) );
    CHECK( lua_tonumber( L, -3 ) == 1 );
    CHECK( lua_tonumber( L, -1 ) == 3 );
    CHECK( calls == 1 );
    lua_settop( L, 0 );
}

TEST_CASE( "A C++ exception in a bound function becomes a script error" )
{
    LuaState state;
    lua_State* L = state.L();
    LuaPushFunction( L, "explode", []() { throw std::runtime_error( "no such level" ); } );
    lua_setglobal( L, "explode" );
    CHECK( ErrorOf( L, "explode()" ).find( "no such level" ) != string::npos );
}

TEST_CASE( "A bound lambda is destroyed with the state" )
{
    auto token = std::make_shared<int>( 0 );
    {
        LuaState state;
        LuaPushFunction( state.L(), "hold", [token]() { return *token; } );
        lua_setglobal( state.L(), "hold" );
        CHECK( token.use_count() == 2 );
    }
    CHECK( token.use_count() == 1 );
}

TEST_CASE( "A script error carries the traceback of where it happened" )
{
    LuaState state;
    auto ran = RunLua( state.L(), R"(
local function inner()
    local t = nil
    return t.x
end
local function outer() return inner() end
outer()
)" );
    REQUIRE_FALSE( ran.has_value() );
    CHECK( ran.error().mMessage.find( "test:4" ) != string::npos );
    CHECK( ran.error().mTraceback.find( "inner" ) != string::npos );
    CHECK( ran.error().mTraceback.find( "outer" ) != string::npos );
    CHECK( lua_gettop( state.L() ) == 0 );
}

TEST_CASE( "A compile error names the line" )
{
    auto bytecode = CompileScript( "local x = 1\nlocal = 2" );
    REQUIRE_FALSE( bytecode.has_value() );
    CHECK( bytecode.error().find( "2" ) != string::npos );
}

TEST_CASE( "Atoms number names once each" )
{
    LuaState state;
    const i16 speed = state.Atom( "speed" );
    CHECK( speed >= 0 );
    CHECK( state.Atom( "speed" ) == speed );
    CHECK( state.Atom( "jump" ) != speed );
    CHECK( state.AtomName( speed ) == "speed" );
    CHECK( state.AtomName( -1 ).empty() );
}

TEST_CASE( "Sealed globals are read-only to scripts" )
{
    LuaState state;
    state.Seal();
    CHECK_FALSE( RunLua( state.L(), "math.pi = 3" ).has_value() );
    CHECK_FALSE( RunLua( state.L(), "print = nil" ).has_value() );
}

namespace
{
struct Light
{
    f32 mBrightness = 1;
    Vec3 mColor;
    int mId = 7;
};

LuaType& RegisterLight( LuaState& state )
{
    return LuaTypeBuilder<Light>( state, "light" )
        .Field( "brightness", &Light::mBrightness )
        .Field( "color", &Light::mColor )
        .ReadOnly( "id", &Light::mId )
        .Method( "dim",
                 []( Light& self, f32 by ) {
                     self.mBrightness -= by;
                     return self.mBrightness;
                 } )
        .Type();
}
}

TEST_CASE( "An engine type's fields read and write by name" )
{
    LuaState state;
    lua_State* L = state.L();
    LuaType& type = RegisterLight( state );
    Light& light = type.PushNew<Light>( L );
    lua_setglobal( L, "lamp" );

    REQUIRE( RunLua( L, R"(
lamp.brightness = lamp.brightness * 3
lamp.color = vector.create( 1, 0.5, 0 )
return lamp:dim( 0.5 ), lamp.id, typeof( lamp )
)",
                     3 ) );
    CHECK( light.mBrightness == 2.5f );
    CHECK( light.mColor.y == 0.5f );
    CHECK( lua_tonumber( L, -3 ) == 2.5 );
    CHECK( lua_tonumber( L, -2 ) == 7 );
    CHECK( string( lua_tostring( L, -1 ) ) == "light" );
    lua_settop( L, 0 );
}

TEST_CASE( "An engine type says what is wrong and what it has" )
{
    LuaState state;
    lua_State* L = state.L();
    RegisterLight( state ).PushNew<Light>( L );
    lua_setglobal( L, "lamp" );

    const string misspelt = ErrorOf( L, "return lamp.brightnes" );
    CHECK( misspelt.find( "light has no field 'brightnes'" ) != string::npos );
    CHECK( misspelt.find( "brightness, color, id" ) != string::npos );
    CHECK( ErrorOf( L, "lamp.id = 3" ).find( "light.id is read-only" ) != string::npos );
    CHECK( ErrorOf( L, "lamp.brightness = 'high'" ).find( "light.brightness: number expected, got string" ) !=
           string::npos );
    CHECK( ErrorOf( L, "lamp:glow()" ).find( "light has no method 'glow' (methods: dim)" ) != string::npos );
    CHECK( ErrorOf( L, "lamp:dim( 'a lot' )" ).find( "number expected" ) != string::npos );
    // The metamethods stay out of reach.
    REQUIRE( RunLua( L, "return getmetatable( lamp )", 1 ) );
    CHECK( string( lua_tostring( L, -1 ) ) == "light" );
    lua_pop( L, 1 );
}

TEST_CASE( "An engine type hands its object back typed, and only as the type it holds" )
{
    LuaState state;
    lua_State* L = state.L();
    LuaType& type = RegisterLight( state );
    type.PushNew<Light>( L ).mId = 12;
    lua_pushnumber( L, 1 );

    const auto light = type.To<Light>( L, 1 );
    REQUIRE( light );
    CHECK( light->mId == 12 );
    CHECK_FALSE( type.To<Light>( L, 2 ) );
    CHECK( state.FindType( type.Tag() ) == &type );
    CHECK( state.FindType( type.Tag() + 1 ) == nullptr );
    // Asking for another C++ type is a bug in the engine, not in a script.
    CHECK_THROWS_AS( (void)type.To<Vec3>( L, 1 ), std::logic_error );
    lua_settop( L, 0 );
}
