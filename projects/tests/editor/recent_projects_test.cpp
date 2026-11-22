// The projects the editor opened, the last first, kept in the user's folder;
// a new project made in a folder of its own, and the editor opening it.
#include "bubble/audio/audio.hpp"
#include "bubble/core/utf8.hpp"
#include "bubble/editor/editor_project.hpp"
#include "bubble/editor/project_picker.hpp"
#include "bubble/editor/recent_projects.hpp"
#include "bubble/types/stream.hpp"
#include "log_watch.hpp"
#include <doctest.h>

using namespace bubble;
using bubble::test::LogWatch;

namespace
{
// A folder of its own under the system's temporary one, empty, gone after.
struct Folder
{
    OsPath mPath;

    explicit Folder( string_view name ) : mPath( fs::temp_directory_path() / PathFromUtf8( name ) )
    {
        fs::remove_all( mPath );
        fs::create_directories( mPath );
    }
    ~Folder() { fs::remove_all( mPath ); }
};
}

TEST_CASE( "A project opened goes first in the list, once, and the oldest past the most are let go" )
{
    RecentProjects recent( nullopt );
    recent.Opened( PathFromUtf8( "C:/games/one" ), "one", 100 );
    recent.Opened( PathFromUtf8( "C:/games/two" ), "two", 200 );
    recent.Opened( PathFromUtf8( "C:/games/one" ), "one again", 300 );
    REQUIRE( recent.All().size() == 2 );
    CHECK( recent.All()[0].mName == "one again" );
    CHECK( recent.All()[0].mOpened == 300 );
    CHECK( recent.All()[1].mName == "two" );

    recent.Forget( recent.All()[1].mPath );
    CHECK( recent.All().size() == 1 );
    for ( int at = 0; at < 40; ++at )
        recent.Opened( PathFromUtf8( format( "C:/games/game_{}", at ) ), "", at );
    CHECK( recent.All().size() == RecentProjects::cMost );
    CHECK( recent.All()[0].mPath.ends_with( "game_39" ) );
}

TEST_CASE( "The recent projects are kept in their file, and a broken one starts the list empty" )
{
    const Folder folder( "bubble_recent_test" );
    const OsPath file = folder.mPath / RecentProjects::cFileName;
    {
        RecentProjects recent( file );
        recent.Opened( folder.mPath / "game", "game", 1234 );
    }
    {
        const RecentProjects recent( file );
        REQUIRE( recent.All().size() == 1 );
        CHECK( recent.All()[0].mName == "game" );
        CHECK( recent.All()[0].mOpened == 1234 );
    }
    ofstream( file, ios::binary | ios::trunc ) << "{ \"projects\": 3 }";
    const LogWatch log;
    const RecentProjects recent( file );
    CHECK( recent.All().empty() );
    CHECK( log.Saw( LogLevel::Error, { "recent.json", "\"projects\": a list expected" } ) );
}

TEST_CASE(
    "A new project is made in a folder of its own and opens on its level; a name or a folder that will not do is said" )
{
    const Folder folder( "bubble_new_project_test" );
    const auto madeMaybe = TryCreateProject( folder.mPath, "my game" );
    REQUIRE_MESSAGE( madeMaybe, madeMaybe.error() );
    CHECK( fs::exists( *madeMaybe / "project.bubble" ) );
    CHECK( fs::exists( *madeMaybe / "levels" / "main.scene" ) );
    CHECK( fs::exists( *madeMaybe / ".gitignore" ) );

    const Scope<AudioDevice> audio = AudioDevice::Offline();
    {
        auto projectMaybe = EditorProject::TryOpen( *madeMaybe, *audio );
        REQUIRE_MESSAGE( projectMaybe, projectMaybe.error() );
        CHECK( ( *projectMaybe )->GetSettings().mName == "my game" );
        const auto openedMaybe = ( *projectMaybe )->TryOpenStartLevel();
        CHECK_MESSAGE( openedMaybe, openedMaybe.error() );
    }

    const auto againMaybe = TryCreateProject( folder.mPath, "my game" );
    REQUIRE_FALSE( againMaybe );
    CHECK_MESSAGE( againMaybe.error().find( "is there already" ) != string::npos, againMaybe.error() );
    const auto slashMaybe = TryCreateProject( folder.mPath, "a/b" );
    REQUIRE_FALSE( slashMaybe );
    CHECK_MESSAGE( slashMaybe.error().find( "is no folder's name" ) != string::npos, slashMaybe.error() );
}

TEST_CASE( "How long ago a project was opened reads as words" )
{
    const i64 day = 86400;
    CHECK( AgoText( 10 * day, 10 * day + 100 ) == "today" );
    CHECK( AgoText( 10 * day, 11 * day ) == "yesterday" );
    CHECK( AgoText( 0, 5 * day ) == "5 days ago" );
    CHECK( AgoText( 0, 90 * day ) == "3 months ago" );
    CHECK( AgoText( 0, 800 * day ) == "2 years ago" );
}
