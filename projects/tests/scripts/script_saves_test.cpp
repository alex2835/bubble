// What a game remembers: `progress` from one level's world to the next,
// and save slots - written whole or not at all, read back with their
// header.
#include "bubble/core/jobs.hpp"
#include "bubble/core/utf8.hpp"
#include "bubble/types/thread.hpp"
#include "scripts/script_world.hpp"
#include <doctest.h>

using namespace bubble;
using namespace bubble::test;

namespace
{
// A world of one entity with the script, loaded and started.
struct Level
{
    Scope<World> mWorld;

    Level( ScriptWorld& w, string_view path, string_view source, WorldOptions options = {} )
    {
        const AssetId script = w.Write( path, source );
        Scene scene;
        ScriptWorld::Add( scene, "it", script );
        mWorld = CreateScope<World>( w.mApp, std::move( scene ), options );
        w.Load( *mWorld );
    }
};
}

TEST_CASE( "progress goes from one level's world to the next, and reads as it is now, not as it was at load" )
{
    ScriptWorld w;
    {
        // One script writes, another reads: Luau must not take progress.coins
        // once, when the code loads.
        const AssetId writer = w.Write( "scripts/writer.luau", R"(
function on_fixed_update( self, dt )
    progress.coins = ( progress.coins or 0 ) + 1
    progress.doors = { north = true }
end
)" );
        const AssetId reader = w.Write(
            "scripts/reader.luau", "function on_fixed_update( self, dt ) record( tostring( progress.coins ) ) end" );
        Scene scene;
        ScriptWorld::Add( scene, "writer", writer );
        ScriptWorld::Add( scene, "reader", reader );
        World first( w.mApp, std::move( scene ) );
        w.Load( first );
        first.Step();
        first.Step();
        CHECK( w.TakeRecorded() == vector<string>{ "1", "2" } );
    }
    Level second( w, "scripts/next.luau", R"(
function on_start( self )
    record( string.format( "%d %s", progress.coins, tostring( progress.doors.north ) ) )
end
)" );
    CHECK( w.TakeRecorded() == vector<string>{ "2 true" } );
}

TEST_CASE( "progress is a table scripts change, not one they replace" )
{
    ScriptWorld w;
    const LogWatch log;
    Level level( w, "scripts/replace.luau", "function on_start( self ) progress = {} end" );
    CHECK( log.Saw( LogLevel::Error, { "assignment to undeclared global 'progress'" } ) );
}

TEST_CASE( "A slot is written with its header and read back; one never written is nil" )
{
    ScriptWorld w;
    const AssetId level = AssetId::Generate();
    Level writing( w, "scripts/saver.luau", R"(
function on_start( self )
    save.write( "slot_1", { coins = 7, tint = color( 1, 0, 0 ), at = vector.create( 1, 2, 3 ) } )
    save.write( "slot_2", {} )
end
)",
                   WorldOptions{ .mLevel = level } );
    Level reading( w, "scripts/loader.luau", R"(
function on_start( self )
    local data = save.read( "slot_1" )
    record( string.format( "%d %s %g", data.coins, tostring( data.tint ), data.at.y ) )
    local info = save.info( "slot_1" )
    record( info.version .. " " .. tostring( info.time > 0 ) .. " " .. tostring( save.read( "slot_9" ) ) )
    record( table.concat( save.list(), "," ) )
end
)" );
    CHECK( w.TakeRecorded() == vector<string>{ "7 color( 1, 0, 0, 1 ) 2", "test 1.0 true nil", "slot_1,slot_2" } );
    const auto bytesMaybe = w.mSaves->TryRead( "slot_1" );
    REQUIRE( bytesMaybe );
    REQUIRE( *bytesMaybe );
    CHECK( TryReadSave( **bytesMaybe )->first.mLevel == level );
}

TEST_CASE( "What cannot be saved, and a slot name a file could not carry, are the script's errors" )
{
    ScriptWorld w;
    const LogWatch log;
    Level one( w, "scripts/a.luau", "function on_start( self ) save.write( 'slot', { hit = function() end } ) end" );
    Level two( w, "scripts/b.luau", "function on_start( self ) save.write( '../slot', {} ) end" );
    CHECK( log.Saw( LogLevel::Error, { "save.write: hit: a function cannot be saved" } ) );
    CHECK( log.Saw( LogLevel::Error, { "save.write: save slot \"../slot\": letters, digits, _ and -, at most 64" } ) );
    CHECK( w.mSaves->List().empty() );
}

TEST_CASE( "A save goes to bytes and back; damaged bytes, or another format, are refused" )
{
    const SaveHeader header{ .mVersion = "1.2", .mLevel = AssetId::Generate(), .mTime = 1700000000.5 };
    const string bytes = WriteSave( header, "data" );
    const auto backMaybe = TryReadSave( bytes );
    REQUIRE( backMaybe );
    CHECK( backMaybe->first.mVersion == "1.2" );
    CHECK( backMaybe->first.mLevel == header.mLevel );
    CHECK( backMaybe->first.mTime == 1700000000.5 );
    CHECK( backMaybe->second == "data" );
    CHECK( TryReadSave( bytes.substr( 0, bytes.size() - 1 ) ).error() == "the save is damaged" );
    CHECK( TryReadSave( "BMDL" ).error() == "not a save the game wrote" );
}

TEST_CASE( "Slots in a folder are written beside and renamed over, off the main thread" )
{
    const OsPath folder = fs::temp_directory_path() / PathFromUtf8( "bubble_saves_test" );
    error_code error;
    fs::remove_all( folder, error );
    Jobs jobs( 2 );
    {
        const Ref<SaveSlots> slots = SaveSlotsInFolder( folder, jobs );
        vector<string> done;
        slots->Write( "slot_1", "first", [&]( expected<void, string> writtenMaybe ) {
            done.push_back( writtenMaybe ? "ok" : writtenMaybe.error() );
        } );
        const auto waitFor = [&]( size_t count ) {
            for ( int tries = 0; tries < 5000 and done.size() < count; ++tries )
            {
                jobs.RunMainQueue();
                this_thread::sleep_for( chrono::milliseconds( 1 ) );
            }
        };
        waitFor( 1 );
        CHECK( done == vector<string>{ "ok" } );
        slots->Write( "slot_1", "second", [&]( expected<void, string> ) { done.push_back( "again" ); } );
        waitFor( 2 );
        REQUIRE( done.size() == 2 );
        CHECK( slots->TryRead( "slot_1" ).value() == opt<string>( "second" ) );
        CHECK( slots->TryRead( "slot_2" ).value() == opt<string>() );
        CHECK( slots->List() == vector<string>{ "slot_1" } );
        CHECK_FALSE( fs::exists( folder / PathFromUtf8( "slot_1.save.writing" ) ) );
    }
    fs::remove_all( folder, error );
}
