// The scan over a project's files: every source gets its id from the .import
// beside it, and the rules for the files people move, copy and break.
#include "bubble/import/import_file.hpp"
#include "bubble/import/project_scan.hpp"
#include "bubble/scene/engine_file.hpp"
#include <doctest.h>

using namespace bubble;

namespace
{
constexpr array<string_view, 2> cSources = { ".luau", ".glb" };

AssetPath Path( string_view text )
{
    return *AssetPath::TryFrom( text );
}

AssetId IdIn( const hmap<string, string>& files, string_view importFile )
{
    return TryParseImportFile( files.at( string( importFile ) ) )->mId;
}
}

TEST_CASE( "A source with no .import gets one with a new id, and keeps it" )
{
    hmap<string, string> files{ { "scripts/player.luau", "" }, { "models/hero.glb", "" }, { "notes.txt", "" } };
    const auto scanMaybe = TryScanProject( ProjectFiles::InMemory( files ), cSources, cEngineFileExtensions );
    REQUIRE( scanMaybe );
    CHECK( scanMaybe->mWarnings.empty() );
    CHECK( scanMaybe->mCatalog.Count() == 2 );
    // Written beside each source; a file of no importer has none.
    REQUIRE( files.contains( "scripts/player.luau.import" ) );
    REQUIRE( files.contains( "models/hero.glb.import" ) );
    CHECK_FALSE( files.contains( "notes.txt.import" ) );
    const AssetId player = IdIn( files, "scripts/player.luau.import" );
    CHECK( scanMaybe->mCatalog.TryIdOf( Path( "scripts/player.luau" ) ) == player );

    // The next scan reads the same ids and writes nothing.
    const hmap<string, string> before = files;
    const auto againMaybe = TryScanProject( ProjectFiles::InMemory( files ), cSources, cEngineFileExtensions );
    REQUIRE( againMaybe );
    CHECK( againMaybe->mCatalog.TryIdOf( Path( "scripts/player.luau" ) ) == player );
    CHECK( files == before );

    // Moved with its .import - by a file manager, by git - it keeps its id.
    files["scripts/hero/player.luau"] = files.extract( "scripts/player.luau" ).mapped();
    files["scripts/hero/player.luau.import"] = files.extract( "scripts/player.luau.import" ).mapped();
    const auto movedMaybe = TryScanProject( ProjectFiles::InMemory( files ), cSources, cEngineFileExtensions );
    REQUIRE( movedMaybe );
    REQUIRE( movedMaybe->mCatalog.TryPathOf( player ) );
    CHECK( *movedMaybe->mCatalog.TryPathOf( player ) == Path( "scripts/hero/player.luau" ) );
}

TEST_CASE( "A file copied with its .import gets a new id, its settings kept, and a warning" )
{
    hmap<string, string> files{ { "models/chair.glb", "" } };
    REQUIRE( TryScanProject( ProjectFiles::InMemory( files ), cSources, cEngineFileExtensions ) );
    const AssetId chair = IdIn( files, "models/chair.glb.import" );
    ImportFile withSettings = *TryParseImportFile( files["models/chair.glb.import"] );
    withSettings.mSettings = *Json::TryParse( R"({ "scale": 2 })" );
    files["models/chair.glb.import"] = WriteImportFile( withSettings );
    files["models/chair_copy.glb"] = "";
    files["models/chair_copy.glb.import"] = files["models/chair.glb.import"];

    const auto scanMaybe = TryScanProject( ProjectFiles::InMemory( files ), cSources, cEngineFileExtensions );
    REQUIRE( scanMaybe );
    // The first in the order of paths keeps it.
    CHECK( scanMaybe->mCatalog.TryIdOf( Path( "models/chair.glb" ) ) == chair );
    const AssetId copy = IdIn( files, "models/chair_copy.glb.import" );
    CHECK( copy != chair );
    CHECK( scanMaybe->mCatalog.TryIdOf( Path( "models/chair_copy.glb" ) ) == copy );
    CHECK( TryParseImportFile( files["models/chair_copy.glb.import"] )->mSettings.Write() ==
           withSettings.mSettings.Write() );
    CHECK( scanMaybe->mWarnings ==
           vector<string>{ "models/chair_copy.glb had the id of models/chair.glb - copied with its .import? - and has "
                           "a new one" } );
}

TEST_CASE( "A broken .import leaves its source out, and one with no source is an orphan" )
{
    const AssetId gone = AssetId::Generate();
    ImportFile orphan;
    orphan.mId = gone;
    hmap<string, string> files{ { "models/hero.glb", "" },
                                { "models/hero.glb.import", "<<<<<<< merge" },
                                { "models/villain.glb.import", WriteImportFile( orphan ) } };
    const auto scanMaybe = TryScanProject( ProjectFiles::InMemory( files ), cSources, cEngineFileExtensions );
    REQUIRE( scanMaybe );
    CHECK( scanMaybe->mCatalog.Count() == 0 );
    REQUIRE( scanMaybe->mWarnings.size() == 1 );
    CHECK( scanMaybe->mWarnings[0].starts_with( "models/hero.glb.import: 1:1: " ) );
    CHECK( scanMaybe->mWarnings[0].ends_with( " - models/hero.glb is left out until it is mended" ) );
    // Left for a person to mend, not overwritten.
    CHECK( files["models/hero.glb.import"] == "<<<<<<< merge" );
    CHECK( scanMaybe->mOrphans == vector<pair<AssetPath, AssetId>>{ { Path( "models/villain.glb.import" ), gone } } );
}

TEST_CASE( "A scan that cannot list, read or write the project fails, saying what" )
{
    const ProjectFiles unlisted( {}, {}, [] { return expected<vector<AssetPath>, string>( unexpected( "gone"s ) ); } );
    CHECK( TryScanProject( unlisted, cSources, cEngineFileExtensions ).error() == "gone" );

    const ProjectFiles readOnly(
        []( const AssetPath& ) { return expected<string, string>( "" ); },
        []( const AssetPath&, string_view ) { return expected<void, string>( unexpected( "read-only"s ) ); },
        [] { return expected<vector<AssetPath>, string>( vector{ Path( "a.luau" ) } ); } );
    CHECK( TryScanProject( readOnly, cSources, cEngineFileExtensions ).error() ==
           "cannot write a.luau.import: read-only" );
}

TEST_CASE( "A level or a prefab keeps its id inside: one without gets one written in, a copy a new one" )
{
    const AssetId street = AssetId::Generate();
    const string withId = R"({ "version": 1, "asset": ")" + street.ToString() + R"(", "root": {} })";
    hmap<string, string> files{ { "levels/street.scene", withId },
                                { "levels/street_copy.scene", withId },
                                { "props/house.prefab", R"({ "version": 1, "next_id": 1, "root": { "id": 0 } })" },
                                { "levels/broken.scene", "{ oops" } };
    const auto scanMaybe = TryScanProject( ProjectFiles::InMemory( files ), cSources, cEngineFileExtensions );
    REQUIRE( scanMaybe );
    CHECK( scanMaybe->mCatalog.TryIdOf( Path( "levels/street.scene" ) ) == street );
    // No .import beside an engine file: the id is inside.
    CHECK_FALSE( files.contains( "props/house.prefab.import" ) );
    const auto houseMaybe = TryReadFileId( files["props/house.prefab"] );
    REQUIRE( houseMaybe );
    REQUIRE( *houseMaybe );
    CHECK( scanMaybe->mCatalog.TryIdOf( Path( "props/house.prefab" ) ) == **houseMaybe );
    const AssetId copy = **TryReadFileId( files["levels/street_copy.scene"] );
    CHECK( copy != street );
    CHECK( scanMaybe->mCatalog.TryIdOf( Path( "levels/street_copy.scene" ) ) == copy );
    CHECK( scanMaybe->mCatalog.Count() == 3 );
    REQUIRE( scanMaybe->mWarnings.size() == 2 );
    CHECK( scanMaybe->mWarnings[0].starts_with( "levels/broken.scene: " ) );
    CHECK( scanMaybe->mWarnings[0].ends_with( " - it is left out until it is mended" ) );
    CHECK( scanMaybe->mWarnings[1] ==
           "levels/street_copy.scene had the id of levels/street.scene - a copy? - and has a new one" );
    CHECK( files["levels/broken.scene"] == "{ oops" );
}
