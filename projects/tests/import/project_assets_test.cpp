// The project kept in step with its files: looks that cost nothing when
// nothing changed, sources imported again when they or their settings
// change, and the registry's holders getting the new version.
#include "bubble/assets/asset_registry.hpp"
#include "bubble/import/import_file.hpp"
#include "bubble/import/project_assets.hpp"
#include "bubble/import/script_importer.hpp"
#include "bubble/scene/engine_file.hpp"
#include "bubble/scripts/script_asset.hpp"
#include "bubble/types/format.hpp"
#include "bubble/types/stream.hpp"
#include <doctest.h>

using namespace bubble;

namespace
{
AssetPath Path( string_view text )
{
    return *AssetPath::TryFrom( text );
}

struct Note
{
    string mText;
};

// .txt into its text and the settings' "suffix" after it.
Importer Suffixing()
{
    return Importer{
        .mExtension = ".txt",
        .mKind = "text",
        .mVersion = 1,
        .mImport = []( string_view source, const Json& settings, const AssetPath& ) -> expected<string, string> {
            string made( source );
            if ( const auto suffixMaybe = settings.TryFind( "suffix" ) )
                made += *suffixMaybe->TryAs<string>();
            return made;
        },
    };
}

struct Project
{
    hmap<string, string> mFiles;
    ProjectAssets mProject;
    Jobs mJobs{ 1 };
    AssetRegistry mAssets{ mProject.GetCatalog(),
                           [this]( const AssetId& assetId, string_view kind ) {
                               return mProject.GetFiles().TryRead( ImportedPath( assetId, kind ) );
                           },
                           mJobs };

    explicit Project( hmap<string, string> files ) : mFiles( std::move( files ) ), mProject( Make( mFiles ) )
    {
        mAssets.RegisterLoader<Note>( "text", []( string_view bytes, const AssetPath& ) {
            return expected<Note, string>( Note{ string( bytes ) } );
        } );
        RegisterScriptLoader( mAssets );
    }

    static ProjectAssets Make( hmap<string, string>& files )
    {
        Importers importers;
        importers.Add( Suffixing() );
        importers.Add( ScriptImporter() );
        return ProjectAssets( ProjectFiles::InMemory( files ), std::move( importers ), cEngineFileExtensions );
    }

    ProjectRefresh Refresh()
    {
        auto refreshMaybe = mProject.TryRefresh();
        REQUIRE_MESSAGE( refreshMaybe, refreshMaybe.error() );
        // As the editor does with what was imported.
        for ( const AssetPath& path : refreshMaybe->mImported )
            REQUIRE( mAssets.TryReload( path ) );
        return std::move( *refreshMaybe );
    }
};
}

TEST_CASE( "The first look imports every source, and the next ones do nothing until a file changes" )
{
    Project project( { { "notes/a.txt", "a" }, { "scripts/door.luau", "local open = false" }, { "readme.md", "#" } } );
    const ProjectRefresh first = project.Refresh();
    CHECK( first.mImported == vector<AssetPath>{ Path( "notes/a.txt" ), Path( "scripts/door.luau" ) } );
    CHECK( first.mErrors.empty() );
    CHECK( project.mProject.GetCatalog().Count() == 2 );
    // The .import files the first look wrote are looked at once more.
    CHECK( project.Refresh().mImported.empty() );
    CHECK( project.Refresh().mImported.empty() );

    project.mFiles["scripts/door.luau"] = "local open = true";
    CHECK( project.Refresh().mImported == vector<AssetPath>{ Path( "scripts/door.luau" ) } );
    // A script that does not compile: an error line, and the cache keeps the
    // version it had.
    project.mFiles["scripts/door.luau"] = "function (";
    const ProjectRefresh broken = project.Refresh();
    CHECK( broken.mImported.empty() );
    REQUIRE( broken.mErrors.size() == 1 );
    CHECK( broken.mErrors[0].starts_with( "scripts/door.luau:1:" ) );
}

TEST_CASE( "Settings changed in an .import import the source again, and its holders get the new version" )
{
    Project project( { { "notes/a.txt", "hello" } } );
    project.Refresh();
    const AssetRef<Note> held = project.mAssets.LoadNow<Note>( Path( "notes/a.txt" ) );
    REQUIRE( held.Ready() );
    CHECK( held.TryGet()->mText == "hello" );
    vector<string> heard;
    project.mAssets.OnChanged( [&]( const AssetEntryBase& entry ) { heard.push_back( entry.Path().String() ); } );

    ImportFile settings = *TryParseImportFile( project.mFiles["notes/a.txt.import"] );
    settings.mSettings = *Json::TryParse( R"({ "suffix": "!" })" );
    project.mFiles["notes/a.txt.import"] = WriteImportFile( settings );
    CHECK( project.Refresh().mImported == vector<AssetPath>{ Path( "notes/a.txt" ) } );
    CHECK( held.TryGet()->mText == "hello!" );
    CHECK( heard == vector<string>{ "notes/a.txt" } );
}

TEST_CASE( "A file that comes is taken in, one moved with its .import keeps its id, one deleted leaves" )
{
    Project project( { { "notes/a.txt", "a" }, { "notes/b.txt", "b" } } );
    project.Refresh();
    const AssetCatalog& catalog = project.mProject.GetCatalog();
    const AssetId b = *catalog.TryIdOf( Path( "notes/b.txt" ) );

    project.mFiles["notes/c.txt"] = "c";
    project.mFiles["old/b.txt"] = project.mFiles.extract( "notes/b.txt" ).mapped();
    project.mFiles["old/b.txt.import"] = project.mFiles.extract( "notes/b.txt.import" ).mapped();
    project.mFiles.erase( "notes/a.txt" );
    const ProjectRefresh refresh = project.Refresh();
    // The one that came is imported; the one that moved is what it was.
    CHECK( refresh.mImported == vector<AssetPath>{ Path( "notes/c.txt" ) } );
    CHECK( catalog.TryIdOf( Path( "old/b.txt" ) ) == b );
    CHECK_FALSE( catalog.TryIdOf( Path( "notes/a.txt" ) ) );
    // Its .import stayed behind: an orphan.
    REQUIRE( refresh.mOrphans.size() == 1 );
    CHECK( refresh.mOrphans[0].first == Path( "notes/a.txt.import" ) );
}

TEST_CASE( "A file written in a project folder has a new stamp" )
{
    const OsPath root = fs::temp_directory_path() / "bubble_project_stamps";
    fs::remove_all( root );
    const ProjectFiles files = ProjectFiles::FromDirectory( root );
    REQUIRE( files.TryWrite( Path( "a.txt" ), "one" ) );
    const auto beforeMaybe = files.TryList();
    REQUIRE( beforeMaybe );
    REQUIRE( files.TryWrite( Path( "a.txt" ), "two, longer" ) );
    const auto afterMaybe = files.TryList();
    REQUIRE( afterMaybe );
    CHECK( beforeMaybe->front().mStamp != afterMaybe->front().mStamp );
    // Listed again with nothing written: the same.
    CHECK( *files.TryList() == *afterMaybe );
    fs::remove_all( root );
}
