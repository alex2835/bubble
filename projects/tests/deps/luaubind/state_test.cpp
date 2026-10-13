// luaubind as its users see it: values, tables, functions, coroutines and
// data, without the stack - which every test checks is left as it was.
#include "deps/luaubind/helpers.hpp"
#include <stdexcept>

using namespace luaubind;
using namespace luaubind::test;

TEST_CASE( "A table reads and writes like a map, nested too" )
{
    LuaState lua;
    LuaTable table = lua.NewTable();
    table["name"] = "chest";
    table["stats"] = lua.NewTable();
    table["stats"]["hp"] = 30;
    table[1] = 10;
    table.Append( 20 );

    CHECK( table["name"].TryAs<string>() == "chest" );
    CHECK( table["stats"]["hp"].TryAs<int>() == 30 );
    CHECK( table.Length() == 2 );
    CHECK( table[2].TryAs<int>() == 20 );
    CHECK( table["missing"].IsNil() );
    // Reads are explicit: not a number is nothing, not a zero.
    CHECK_FALSE( table["name"].TryAs<int>() );
    CHECK( table["missing"]["deeper"].IsNil() );
    CHECK( Balanced( lua ) );
}

TEST_CASE( "Get goes through metatables, RawGet does not; a clone is separate" )
{
    LuaState lua;
    LuaTable table = lua.NewTable();
    LuaTable meta = lua.NewTable();
    meta["__index"] = Evaluate( lua, "return { fallback = true }" );
    table.SetMetatable( meta );
    CHECK( table["fallback"].Truthy() );
    CHECK( table.RawGet( "fallback" ).IsNil() );

    table["name"] = "chest";
    LuaTable clone = table.Clone();
    clone["name"] = "copy";
    CHECK( table["name"].TryAs<string>() == "chest" );
    CHECK( Balanced( lua ) );
}

TEST_CASE( "Pairs takes every entry, and the loop may change the table" )
{
    LuaState lua;
    LuaTable table( Evaluate( lua, "return { a = 1, b = 2, c = 3 }" ) );
    int sum = 0;
    for ( const auto& [key, value] : table.Pairs() )
    {
        sum += value.TryAs<int>().value_or( 0 );
        table.RawSet( key, LuaValue() );
    }
    CHECK( sum == 6 );
    CHECK( table.Pairs().empty() );
    CHECK( Balanced( lua ) );
}

TEST_CASE( "A frozen table refuses writes, from scripts and from C++" )
{
    LuaState lua;
    LuaTable table = lua.NewTable();
    table.Freeze();
    CHECK( table.Frozen() );
    LuaFunction write( Evaluate( lua, "return function( t ) t.x = 1 end" ) );
    CHECK_FALSE( write( table ) );
    CHECK_THROWS_AS( table["x"] = 1, std::logic_error );
    CHECK( Balanced( lua ) );
}

TEST_CASE( "Typed values are what they say or nil" )
{
    LuaState lua;
    CHECK( LuaTable( lua.Value( 5 ) ).IsNil() );
    CHECK( LuaFunction( lua.NewTable() ).IsNil() );
    CHECK( lua.Value( 5 ).TryAs<int>() == 5 );
    CHECK( lua.Value( "word" ).TryAs<string>() == "word" );
    CHECK_FALSE( lua.Value( 2.5 ).TryAs<int>() );
    CHECK( lua.Value( true ).Is( LuaKind::Boolean ) );
    CHECK( lua.Value( "x" ).Describe() == "\"x\"" );
    CHECK( LuaValue().Describe() == "nil" );
    // A write through nil is a bug, not a silent no-op.
    CHECK_THROWS_AS( LuaTable()["x"] = 1, std::logic_error );
    CHECK( Balanced( lua ) );
}

TEST_CASE( "A function is called like one and gives back its first result or the error" )
{
    LuaState lua;
    LuaFunction add( Evaluate( lua, "return function( a, b, c ) return a + b + ( c or 0 ), 'ignored' end" ) );
    CHECK( add( 1, 2 )->TryAs<int>() == 3 );
    CHECK( add( 1, LuaRest{ { lua.Value( 2 ), lua.Value( 4 ) } } )->TryAs<int>() == 7 );

    auto failedMaybe = add( "one", 2 );
    REQUIRE_FALSE( failedMaybe );
    CHECK( failedMaybe.error().mMessage.find( "attempt to perform arithmetic" ) != string::npos );
    CHECK_FALSE( LuaFunction()() );
    CHECK( Balanced( lua ) );
}

TEST_CASE( "A lambda stored in a table is a function, named after its key" )
{
    LuaState lua;
    LuaTable globals = lua.Globals();
    globals["greet"] = []( const string& name ) { return "hello, " + name; };
    CHECK( Evaluate( lua, "return greet( 'luau' )" ).TryAs<string>() == "hello, luau" );

    globals["fail"] = []() { throw LuaError( "no" ); };
    auto chunkMaybe = lua.TryLoad( "=named", Bytecode( "fail()" ), globals );
    REQUIRE( chunkMaybe );
    auto calledMaybe = ( *chunkMaybe )();
    REQUIRE_FALSE( calledMaybe );
    CHECK( calledMaybe.error().mTraceback.find( "fail" ) != string::npos );
}

TEST_CASE( "A bound function's LuaError points at the script line that called it" )
{
    LuaState lua;
    lua.Globals()["check"] = []( int value ) {
        if ( value < 0 )
            throw LuaError( "check needs a positive number" );
        return value * 2;
    };
    auto chunkMaybe =
        lua.TryLoad( "scripts/use.luau", Bytecode( "local x = check( 2 )\nreturn check( -1 )" ), lua.Globals() );
    REQUIRE( chunkMaybe );
    auto calledMaybe = ( *chunkMaybe )();
    REQUIRE_FALSE( calledMaybe );
    CHECK( calledMaybe.error().mMessage == "scripts/use.luau:2: check needs a positive number" );
    CHECK( Balanced( lua ) );
}

TEST_CASE( "A bound function takes the rest of the arguments as LuaRest" )
{
    LuaState lua;
    size_t seen = 0;
    lua.Globals()["count"] = [&]( const string&, const LuaRest& rest ) { seen = rest.mValues.size(); };
    Evaluate( lua, "count( 'x', 1, 2, 3 ) return nil" );
    CHECK( seen == 3 );
    Evaluate( lua, "count( 'x' ) return nil" );
    CHECK( seen == 0 );
}

TEST_CASE( "A coroutine yields through LuaYield and resumes with values" )
{
    LuaState lua;
    lua.Globals()["pause"] = [&]( int seconds ) {
        if ( not lua.Yieldable() )
            throw LuaError( "pause works in a coroutine" );
        return LuaYield{ lua.Value( seconds ) };
    };
    LuaFunction body( Evaluate( lua, R"(return function( start )
        local got = pause( start )
        local again = pause( got + 1 )
        return again
    end)" ) );
    LuaThread thread = lua.NewThread( body );

    LuaResume first = thread.Resume( 5 );
    REQUIRE( first.mStatus == LuaResume::Status::Waiting );
    CHECK( first.mWait.TryAs<int>() == 5 );
    LuaResume second = thread.Resume( 10 );
    REQUIRE( second.mStatus == LuaResume::Status::Waiting );
    CHECK( second.mWait.TryAs<int>() == 11 );
    CHECK( thread.Resume( 0 ).mStatus == LuaResume::Status::Finished );

    // Outside a coroutine the same function refuses.
    CHECK_FALSE( LuaFunction( Evaluate( lua, "return function() pause( 1 ) end" ) )() );
    CHECK( Balanced( lua ) );
}

TEST_CASE( "A failing coroutine says where, with its traceback" )
{
    LuaState lua;
    auto chunkMaybe =
        lua.TryLoad( "scripts/co.luau", Bytecode( "return function()\n  error( 'deep' )\nend" ), lua.Globals() );
    REQUIRE( chunkMaybe );
    LuaThread thread = lua.NewThread( LuaFunction( *( *chunkMaybe )() ) );
    LuaResume resumed = thread.Resume();
    REQUIRE( resumed.mStatus == LuaResume::Status::Failed );
    CHECK( resumed.mError.mMessage == "scripts/co.luau:2: deep" );
    CHECK( resumed.mError.mTraceback.find( "scripts/co.luau:2" ) != string::npos );
    CHECK( Balanced( lua ) );
}

TEST_CASE( "C++ called from a coroutine works on the coroutine's stack" )
{
    LuaState lua;
    // The bound function calls back into Luau and builds a table: both on
    // the thread running it, not the main one.
    lua.Globals()["twice"] = [&]( const LuaFunction& fn ) {
        LuaTable results = lua.NewTable();
        results.Append( *fn() );
        results.Append( *fn() );
        return results;
    };
    lua.Globals()["pause"] = [&]() { return LuaYield{ lua.Value( 0 ) }; };
    LuaFunction body( Evaluate( lua, R"(return function()
        local n = 0
        pause()
        local results = twice( function() n += 1; return n end )
        return results[1] + results[2]
    end)" ) );
    LuaThread thread = lua.NewThread( body );
    REQUIRE( thread.Resume().mStatus == LuaResume::Status::Waiting );
    CHECK( thread.Resume().mStatus == LuaResume::Status::Finished );
    CHECK( lua.Active() == lua.L() );
    CHECK( Balanced( lua ) );
}

TEST_CASE( "CallerEnvironment is the environment of the file that called" )
{
    LuaState lua;
    LuaTable seen;
    lua.Globals()["whoami"] = [&]() { seen = lua.CallerEnvironment(); };
    LuaTable environment = lua.NewTable();
    LuaTable meta = lua.NewTable();
    meta["__index"] = lua.Globals();
    environment.SetMetatable( meta );
    auto chunkMaybe = lua.TryLoad( "=file", Bytecode( "whoami()" ), environment );
    REQUIRE( chunkMaybe );
    REQUIRE( ( *chunkMaybe )() );
    CHECK( seen == environment );
}

TEST_CASE( "A weak-keyed table lets its keys go" )
{
    LuaState lua;
    LuaTable weak = lua.NewWeakKeyTable();
    {
        LuaTable key = lua.NewTable();
        weak[key] = true;
        CHECK( weak[key].Truthy() );
    }
    lua_gc( lua.L(), LUA_GCCOLLECT, 0 );
    CHECK( weak.Pairs().empty() );
}

TEST_CASE( "Data goes to bytes and back, and copies deep" )
{
    LuaState lua;
    LuaTable data( Evaluate( lua, "return { name = 'hero', stats = { hp = 3 } }" ) );
    auto bytesMaybe = lua.TryEncode( data );
    REQUIRE( bytesMaybe );
    auto backMaybe = lua.TryDecode( *bytesMaybe );
    REQUIRE( backMaybe );
    CHECK( LuaTable( *backMaybe )["stats"]["hp"].TryAs<int>() == 3 );

    auto copyMaybe = lua.TryDeepCopy( data );
    REQUIRE( copyMaybe );
    CHECK_FALSE( LuaTable( *copyMaybe )["stats"].Value() == data["stats"].Value() );
    CHECK_FALSE( lua.TryEncode( Evaluate( lua, "return { f = print }" ) ) );
    CHECK( Balanced( lua ) );
}

TEST_CASE( "Globals are written before sealing, never after" )
{
    LuaState lua;
    lua.Globals()["answer"] = 42;
    lua.Seal();
    CHECK( lua.Globals()["answer"].TryAs<int>() == 42 );
    CHECK_THROWS_AS( lua.Globals()["late"] = 1, std::logic_error );
}

TEST_CASE( "print goes to the handler" )
{
    string printed;
    LuaState lua( [&]( string_view text ) { printed = text; } );
    Evaluate( lua, "print( 'hp', 10, true ) return nil" );
    CHECK( printed == "hp\t10\ttrue" );
}
