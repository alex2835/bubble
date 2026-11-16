// Script data as bytes (saves, progress) and copied in place (snapshots).
#include "deps/luaubind/helpers.hpp"

using namespace luaubind;
using namespace luaubind::test;

namespace
{
// Bytes of the format's own shape: a chain of tables one deep, each holding the
// next under "inner", written as the writer would write them.
void PutNumber32( string& out, u32 value )
{
    for ( int byte = 0; byte < 4; ++byte )
        out.push_back( static_cast<char>( ( value >> ( 8 * byte ) ) & 0xFF ) );
}

string NestedBytes( int tables )
{
    string bytes;
    bytes.push_back( static_cast<char>( 2 ) ); // the format itself
    for ( int table = 0; table < tables; ++table )
    {
        bytes.push_back( static_cast<char>( 6 ) ); // a table
        PutNumber32( bytes, 1 );                   // with one entry
        bytes.push_back( static_cast<char>( 4 ) ); // a string key
        PutNumber32( bytes, 5 );
        bytes += "inner";
    }
    bytes.push_back( static_cast<char>( 0 ) ); // the innermost value
    return bytes;
}

// A table inside another `depth` times, written out as Luau source.
string NestedSource( int depth )
{
    string source;
    for ( int i = 0; i < depth; ++i )
        source += "{ inner = ";
    source += "1";
    for ( int i = 0; i < depth; ++i )
        source += " }";
    return source;
}

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

TEST_CASE( "Tables nested past the limit are refused going out and coming back" )
{
    LuaState state;
    lua_State* L = state.L();

    Push( L, NestedSource( 100 ) );
    auto deepMaybe = TryLuaEncode( L, -1 );
    REQUIRE_MESSAGE( deepMaybe.has_value(), deepMaybe.error() );
    lua_pop( L, 1 );
    Push( L, NestedSource( 101 ) );
    auto pastMaybe = TryLuaEncode( L, -1 );
    REQUIRE_FALSE( pastMaybe );
    CHECK( pastMaybe.error().ends_with( ": tables nested deeper than 100" ) );
    lua_pop( L, 1 );

    // Bytes of the same depth are refused on the way back, and the stack the
    // reader was filling is left as it was found.
    CHECK( TryLuaDecode( L, NestedBytes( 101 ) ).error() == "the data is damaged: nested too deep" );
    CHECK( lua_gettop( L ) == 0 );
    // One table under the limit reads back.
    REQUIRE( TryLuaDecode( L, NestedBytes( 100 ) ) );
    string deepest = "value";
    for ( int level = 0; level < 99; ++level )
        deepest += ".inner";
    CHECK( Holds( L, "type( " + deepest + " ) == 'table' and " + deepest + ".inner == nil" ) );
    CHECK( lua_gettop( L ) == 0 );
}

TEST_CASE( "Keys of any kind, and a string with a zero byte in it, come back as they were" )
{
    LuaState state;
    lua_State* L = state.L();
    Push( L, R"({
        [7] = "seven", ["7"] = "text seven", [1.5] = "a half", [true] = "yes", [false] = "no",
        [""] = "empty", ["a\0b"] = "a zero in a key", raw = "x\0y",
    })" );
    auto bytesMaybe = TryLuaEncode( L, -1 );
    REQUIRE_MESSAGE( bytesMaybe.has_value(), bytesMaybe.error() );
    lua_pop( L, 1 );

    REQUIRE( TryLuaDecode( L, *bytesMaybe ) );
    CHECK( Holds( L, R"(value[7] == "seven" and value["7"] == "text seven" and value[1.5] == "a half"
        and value[true] == "yes" and value[false] == "no" and value[""] == "empty"
        and value["a\0b"] == "a zero in a key" and #value.raw == 3 and string.byte( value.raw, 2 ) == 0)" ) );
    CHECK( lua_gettop( L ) == 0 );
}

TEST_CASE( "A key that is not a number, text or a bool is refused, saying what it is" )
{
    LuaState state;
    lua_State* L = state.L();
    Push( L, "(function() local t = {}; t[{}] = 'only'; return t end)()" );
    auto tableKeyMaybe = TryLuaEncode( L, -1 );
    REQUIRE_FALSE( tableKeyMaybe );
    CHECK( tableKeyMaybe.error() == "value: a table key cannot be saved" );
    CHECK( lua_gettop( L ) == 1 );
    lua_pop( L, 1 );

    Push( L, "{ [print] = 'no' }" );
    auto keyMaybe = TryLuaEncode( L, -1 );
    REQUIRE_FALSE( keyMaybe );
    CHECK_MESSAGE( keyMaybe.error() == "value: a function key cannot be saved", keyMaybe.error() );
    CHECK( lua_gettop( L ) == 1 );
    lua_pop( L, 1 );

    // A vector is a value the format keeps, and not a key it can write down.
    Push( L, "{ [vector.create( 1, 2, 3 )] = 'no' }" );
    auto vectorKeyMaybe = TryLuaEncode( L, -1 );
    REQUIRE_FALSE( vectorKeyMaybe );
    CHECK_MESSAGE( vectorKeyMaybe.error() == "value: a vector key cannot be saved", vectorKeyMaybe.error() );
    CHECK( lua_gettop( L ) == 1 );
    lua_pop( L, 1 );
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

namespace
{
struct Coin
{
    int mValue = 0;
};

struct Lamp
{
    int mWatts = 0;
};
}

template <>
struct luaubind::LuaUserType<Coin>
{
    static constexpr const char* cName = "coin";
};

namespace
{
void RegisterCoin( LuaState& state )
{
    LuaTypeBuilder<Coin>( state, "coin" )
        .ReadOnly( "value", &Coin::mValue )
        .Saved( []( const Coin& coin ) { return std::to_string( coin.mValue ); },
                []( string_view bytes ) -> expected<Coin, string> {
                    if ( bytes == "13" )
                        return std::unexpected( "an unlucky coin"s );
                    return Coin{ std::stoi( string( bytes ) ) };
                } );
}
}

TEST_CASE( "An engine type that says how it saves goes to bytes and comes back, in another state too" )
{
    LuaState saving;
    RegisterCoin( saving );
    saving.Globals()["coin"] = []( int value ) { return Coin{ value }; };
    const auto bytesMaybe = saving.TryEncode( Evaluate( saving, "return { purse = { coin( 5 ), coin( 7 ) } }" ) );
    REQUIRE_MESSAGE( bytesMaybe, bytesMaybe.error() );

    LuaState loading;
    RegisterCoin( loading );
    const auto valueMaybe = loading.TryDecode( *bytesMaybe );
    REQUIRE_MESSAGE( valueMaybe, valueMaybe.error() );
    loading.Globals()["data"] = *valueMaybe;
    CHECK( Evaluate( loading, "return data.purse[1].value + data.purse[2].value" ).TryAs<int>() == 12 );

    // A state that does not know the type cannot read it, nor a value it refuses.
    LuaState stranger;
    CHECK( stranger.TryDecode( *bytesMaybe ).error() == "the data holds a coin, which is not saved here" );
    const auto unluckyMaybe = saving.TryEncode( Evaluate( saving, "return coin( 13 )" ) );
    REQUIRE( unluckyMaybe );
    CHECK( loading.TryDecode( *unluckyMaybe ).error() == "a coin: an unlucky coin" );
    CHECK( Balanced( saving ) );
    CHECK( Balanced( loading ) );
}

TEST_CASE( "An engine type that does not say how it saves cannot be saved, and the error names it" )
{
    LuaState state;
    LuaType& type = LuaTypeBuilder<Lamp>( state, "lamp" ).Type();
    state.Globals()["lamp"] = type.New( Lamp{ 60 } );
    const auto bytesMaybe = state.TryEncode( Evaluate( state, "return { hall = { lamp } }" ) );
    REQUIRE_FALSE( bytesMaybe );
    CHECK( bytesMaybe.error() == "hall[1]: a lamp cannot be saved" );
    CHECK( Balanced( state ) );
}
