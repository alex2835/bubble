// The import cache: what importers made of the sources, made again when the
// source, its settings or the importer change.
#include "bubble/assets/asset_registry.hpp"
#include "bubble/import/import_cache.hpp"
#include "bubble/import/import_file.hpp"
#include "bubble/import/project_scan.hpp"
#include "bubble/import/script_importer.hpp"
#include "bubble/scripts/script_asset.hpp"
#include "bubble/types/format.hpp"
#include <doctest.h>

using namespace bubble;

namespace
{
AssetPath Path( string_view text )
{
    return *AssetPath::TryFrom( text );
}

// .txt into its text shouted, with the settings' "suffix" after it; refuses
// "broken". Counts what it does.
struct Shouting
{
    u32 mRuns = 0;

    Importer Make( u32 version = 1 )
    {
        return Importer{
            .mExtension = ".txt",
            .mKind = "text",
            .mVersion = version,
            .mImport = [this]( string_view source, const Json& settings,
                               const AssetPath& path ) -> expected<string, string> {
                ++mRuns;
                if ( source == "broken" )
                    return unexpected( format( "{}:1: cannot shout that", path.View() ) );
                string loud;
                for ( const char c : source )
                    loud += static_cast<char>( c >= 'a' and c <= 'z' ? c - 'a' + 'A' : c );
                if ( const auto suffixMaybe = settings.TryFind( "suffix" ) )
                    loud += *suffixMaybe->TryAs<string>();
                return loud;
            },
        };
    }
};

struct Project
{
    hmap<string, string> mFiles{ { "notes/a.txt", "hello" } };
    ProjectFiles mProject = ProjectFiles::InMemory( mFiles );
    AssetId mId;

    Project()
    {
        constexpr array<string_view, 1> cSources = { ".txt" };
        const auto scanMaybe = TryScanProject( mProject, cSources, {} );
        REQUIRE( scanMaybe );
        mId = *scanMaybe->mCatalog.TryIdOf( Path( "notes/a.txt" ) );
    }

    expected<ImportOutcome, string> Import( const Importer& importer, string_view settings = "{}" )
    {
        return TryImportAsset( mProject, importer, Path( "notes/a.txt" ), mId, *Json::TryParse( settings ) );
    }

    string Imported() { return mFiles[ImportedPath( mId, "text" ).String()]; }
};
}

TEST_CASE( "A source is imported once, and again when the source, its settings or the importer change" )
{
    Shouting shouting;
    Project project;
    const Importer importer = shouting.Make();
    CHECK( project.Import( importer ) == ImportOutcome::Imported );
    CHECK( project.Imported() == "HELLO" );
    CHECK( project.Import( importer ) == ImportOutcome::UpToDate );
    CHECK( shouting.mRuns == 1 );

    project.mFiles["notes/a.txt"] = "hi";
    CHECK( project.Import( importer ) == ImportOutcome::Imported );
    CHECK( project.Imported() == "HI" );
    CHECK( project.Import( importer, R"({ "suffix": "!" })" ) == ImportOutcome::Imported );
    CHECK( project.Imported() == "HI!" );
    CHECK( project.Import( importer, R"({ "suffix": "!" })" ) == ImportOutcome::UpToDate );
    CHECK( project.Import( shouting.Make( 2 ), R"({ "suffix": "!" })" ) == ImportOutcome::Imported );
    CHECK( shouting.mRuns == 4 );
    // The cache is no source: a scan does not take it in.
    CHECK( TryScanProject( project.mProject, array<string_view, 1>{ ".text" }, {} )->mCatalog.Count() == 0 );
}

TEST_CASE( "A source its importer refuses leaves the cache as it was, and is tried again when it changes" )
{
    Shouting shouting;
    Project project;
    const Importer importer = shouting.Make();
    REQUIRE( project.Import( importer ) );
    project.mFiles["notes/a.txt"] = "broken";
    CHECK( project.Import( importer ).error() == "notes/a.txt:1: cannot shout that" );
    CHECK( project.Imported() == "HELLO" );
    // Still broken, still tried: it was never made.
    CHECK_FALSE( project.Import( importer ) );
    project.mFiles["notes/a.txt"] = "fixed";
    CHECK( project.Import( importer ) == ImportOutcome::Imported );
    CHECK( project.Imported() == "FIXED" );
}

TEST_CASE( "A project's import takes every source with an importer, in order, and lists what failed" )
{
    Shouting shouting;
    Importers importers;
    importers.Add( shouting.Make() );
    importers.Add( ScriptImporter() );
    CHECK_THROWS_AS( importers.Add( shouting.Make() ), logic_error );
    hmap<string, string> files{ { "b.txt", "broken" },
                                { "a.txt", "a" },
                                { "scripts/door.luau", "local open = false" },
                                { "scripts/bad.luau", "function (" },
                                { "readme.md", "#" } };
    const ProjectFiles project = ProjectFiles::InMemory( files );
    const auto scanMaybe = TryScanProject( project, importers.Extensions(), {} );
    REQUIRE( scanMaybe );

    const ProjectImport first = ImportProject( project, scanMaybe->mCatalog, importers );
    CHECK( first.mImported == vector<AssetPath>{ Path( "a.txt" ), Path( "scripts/door.luau" ) } );
    REQUIRE( first.mErrors.size() == 2 );
    CHECK( first.mErrors[0] == "b.txt:1: cannot shout that" );
    CHECK( first.mErrors[1].starts_with( "scripts/bad.luau:1:" ) );

    // What the script importer made, the engine's loader reads.
    Jobs jobs( 1 );
    AssetRegistry assets(
        scanMaybe->mCatalog,
        [&]( const AssetId& assetId, string_view kind ) { return project.TryRead( ImportedPath( assetId, kind ) ); },
        jobs );
    RegisterScriptLoader( assets );
    CHECK( assets.LoadNow<ScriptAsset>( Path( "scripts/door.luau" ) ).Ready() );

    // Again: nothing changed, nothing imported, the broken ones still broken.
    const ProjectImport second = ImportProject( project, scanMaybe->mCatalog, importers );
    CHECK( second.mImported.empty() );
    CHECK( second.mErrors.size() == 2 );
}
