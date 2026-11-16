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
// A value type of the engine's, crossing by value: an id and a name.
struct Badge
{
    int mId = 0;
    string mName;

    friend bool operator==( const Badge&, const Badge& ) = default;
};

// No ==, so two of them are two values.
struct Ticket
{
    int mSeat = 0;
};
}

template <>
struct luaubind::LuaUserType<Badge>
{
    static constexpr const char* cName = "badge";
};

namespace
{
struct Vec3
{
    f32 x = 0, y = 0, z = 0;
};

string ErrorOf( lua_State* L, string_view source )
{
    auto ranMaybe = TryRunLua( L, source );
    REQUIRE_FALSE( ranMaybe.has_value() );
    return ranMaybe.error().mMessage;
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
    CHECK( state.RawGet( copy, "answer" ).TryAs<int>() == 42 );
    // The same table, not a second one.
    LuaValue again = copy;
    state.RawSet( again, "answer", 7 );
    CHECK( state.RawGet( copy, "answer" ).TryAs<int>() == 7 );
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

    REQUIRE( TryRunLua( L, "return scale( 1.5, 4 )", 1 ) );
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

    REQUIRE( TryRunLua( L, "return split( vector.create( 1, 2, 3 ) )", 3 ) );
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
    auto ranMaybe = TryRunLua( state.L(), R"(
local function inner()
    local t = nil
    return t.x
end
local function outer() return inner() end
outer()
)" );
    REQUIRE_FALSE( ranMaybe.has_value() );
    CHECK( ranMaybe.error().mMessage.find( "test:4" ) != string::npos );
    CHECK( ranMaybe.error().mTraceback.find( "inner" ) != string::npos );
    CHECK( ranMaybe.error().mTraceback.find( "outer" ) != string::npos );
    CHECK( lua_gettop( state.L() ) == 0 );
}

TEST_CASE( "A compile error names the line" )
{
    auto bytecodeMaybe = TryCompileScript( "local x = 1\nlocal = 2" );
    REQUIRE_FALSE( bytecodeMaybe.has_value() );
    CHECK( bytecodeMaybe.error().find( "2" ) != string::npos );
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
    CHECK_FALSE( TryRunLua( state.L(), "math.pi = 3" ).has_value() );
    CHECK_FALSE( TryRunLua( state.L(), "print = nil" ).has_value() );
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

    REQUIRE( TryRunLua( L, R"(
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
    REQUIRE( TryRunLua( L, "return getmetatable( lamp )", 1 ) );
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
    CHECK( state.TryFindType( type.Tag() ) == &type );
    CHECK( state.TryFindType( type.Tag() + 1 ) == nullptr );
    // Asking for another C++ type is a bug in the engine, not in a script.
    CHECK_THROWS_AS( (void)type.To<Vec3>( L, 1 ), std::logic_error );
    lua_settop( L, 0 );
}

namespace
{
LuaType& RegisterBadge( LuaState& state )
{
    return LuaTypeBuilder<Badge>( state, "badge" )
        .ReadOnly( "id", &Badge::mId )
        .Property(
            "name", []( Badge& self ) { return self.mName; },
            []( Badge& self, string name ) {
                if ( name.empty() )
                    throw LuaError( "a badge's name is not empty" );
                self.mName = std::move( name );
            } )
        .Property( "shout", []( Badge& self ) { return self.mName + "!"; } )
        .Method( "check",
                 []( Badge& self, int id ) {
                     if ( id != self.mId )
                         throw LuaError( "not this badge" );
                     return true;
                 } )
        .ToString( []( const Badge& self ) { return "badge " + self.mName; } )
        .Saved( []( const Badge& self ) { return std::to_string( self.mId ) + ":" + self.mName; },
                []( string_view bytes ) -> expected<Badge, string> {
                    const size_t colon = bytes.find( ':' );
                    if ( colon == string_view::npos )
                        return std::unexpected( "no id"s );
                    return Badge{ std::stoi( string( bytes.substr( 0, colon ) ) ),
                                  string( bytes.substr( colon + 1 ) ) };
                } )
        .Type();
}
}

TEST_CASE( "A user type crosses by value: as an argument, a result, a field, and equal by its value" )
{
    LuaState state;
    RegisterBadge( state );
    LuaTable globals = state.Globals();
    globals["make"] = []( int id, string name ) { return Badge{ id, std::move( name ) }; };
    Badge seen;
    globals["see"] = [&]( const Badge& badge ) { seen = badge; };

    const LuaValue result = Evaluate( state, R"(
local a = make( 1, "ann" )
local b = make( 1, "ann" )
see( a )
return { same = a == b, other = a == make( 2, "bo" ), kind = typeof( a ), text = tostring( a ), shout = a.shout }
)" );
    const LuaTable table( result );
    CHECK( table["same"].TryAs<bool>() == true );
    CHECK( table["other"].TryAs<bool>() == false );
    CHECK( table["kind"].TryAs<string>() == "badge" );
    CHECK( table["text"].TryAs<string>() == "badge ann" );
    CHECK( table["shout"].TryAs<string>() == "ann!" );
    CHECK( seen == Badge{ 1, "ann" } );
    // A value held from C++ reads back as the type.
    CHECK( state.Value( Badge{ 3, "cy" } ).TryAs<Badge>() == Badge{ 3, "cy" } );
    CHECK_FALSE( state.Value( 3 ).TryAs<Badge>().has_value() );
    CHECK( Balanced( state ) );
}

TEST_CASE( "A user type's errors are the script's, at its line, and a wrong argument names the type" )
{
    LuaState state;
    RegisterBadge( state );
    LuaTable globals = state.Globals();
    globals["make"] = []( int id, string name ) { return Badge{ id, std::move( name ) }; };
    globals["see"] = []( const Badge& ) {};
    lua_State* L = state.L();

    CHECK( ErrorOf( L, "see( 3 )" ).find( "badge expected" ) != string::npos );
    CHECK( ErrorOf( L, "local b = make( 1, 'a' )\nb.name = ''" ) == "test:2: a badge's name is not empty" );
    CHECK( ErrorOf( L, "local b = make( 1, 'a' )\nb:check( 2 )" ) == "test:2: not this badge" );
    CHECK( ErrorOf( L, "make( 1, 'a' ).name = 5" ).find( "badge.name: string expected, got number" ) != string::npos );
    CHECK( ErrorOf( L, "make( 1, 'a' ).shout = 'x'" ).find( "badge.shout is read-only" ) != string::npos );
    CHECK( Balanced( state ) );
}

TEST_CASE( "A user type's method runs on the coroutine that called it" )
{
    LuaState state;
    lua_State* seenOn = nullptr;
    LuaTypeBuilder<Badge>( state, "badge" ).Method( "where", [&]( Badge& ) { seenOn = state.Active(); } );
    state.Globals()["make"] = []() { return Badge{}; };
    const LuaFunction body( Evaluate( state, "return function() make():where() end" ) );
    const LuaThread thread = state.NewThread( body );
    REQUIRE( thread.Resume().mStatus == LuaResume::Status::Finished );
    CHECK( seenOn != nullptr );
    CHECK( seenOn != state.L() );
}

TEST_CASE( "A user type crossing into a state that has not registered it is the engine's mistake" )
{
    LuaState state;
    CHECK_THROWS_AS( (void)state.Value( Badge{} ), std::logic_error );
    RegisterBadge( state );
    // One Luau type for each C++ type that crosses by value.
    CHECK_THROWS_AS( LuaTypeBuilder<Badge>( state, "other_badge" ), std::logic_error );
}

TEST_CASE( "A type with no == is equal only to itself, and a type's C++ objects come back from values" )
{
    LuaState state;
    LuaType& type = LuaTypeBuilder<Ticket>( state, "ticket" ).Field( "seat", &Ticket::mSeat ).Type();
    const LuaValue ticket = type.New( Ticket{ 4 } );
    state.Globals()["a"] = ticket;
    state.Globals()["b"] = type.New( Ticket{ 4 } );
    const LuaValue result = Evaluate( state, "return { a == a, a == b, tostring( a ) }" );
    const LuaTable table( result );
    CHECK( table[1].TryAs<bool>() == true );
    CHECK( table[2].TryAs<bool>() == false );
    CHECK( table[3].TryAs<string>() == "ticket" );
    REQUIRE( type.TryTo<Ticket>( ticket ) != nullptr );
    CHECK( type.TryTo<Ticket>( ticket )->mSeat == 4 );
    CHECK( type.TryTo<Ticket>( state.Value( 1 ) ) == nullptr );
    CHECK( Balanced( state ) );
}
