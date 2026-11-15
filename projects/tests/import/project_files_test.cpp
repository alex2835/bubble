// The project's files on disk as the editor writes them: whole or not at
// all, folders made on the way, names in any script.
#include "bubble/core/utf8.hpp"
#include "bubble/import/project_files.hpp"
#include "bubble/types/stream.hpp"
#include <doctest.h>

using namespace bubble;

TEST_CASE( "A file written to a project folder reads back, its folders made and the old one replaced" )
{
    const OsPath root = fs::temp_directory_path() / PathFromUtf8( "bubble_project_уровни" );
    fs::remove_all( root );
    const ProjectFiles files = ProjectFiles::FromDirectory( root );
    const AssetPath path = *AssetPath::TryFrom( "levels/первый.scene" );

    CHECK( files.TryRead( path ).error() == "no such file" );
    REQUIRE( files.TryWrite( path, "one" ) );
    CHECK( *files.TryRead( path ) == "one" );
    REQUIRE( files.TryWrite( path, "two, longer" ) );
    CHECK( *files.TryRead( path ) == "two, longer" );
    // Nothing is left beside it.
    size_t count = 0;
    for ( const auto& entry : fs::directory_iterator( root / "levels" ) )
    {
        static_cast<void>( entry );
        ++count;
    }
    CHECK( count == 1 );
    fs::remove_all( root );
}

TEST_CASE( "A project folder lists its files in order, without dot folders or what a cut-short write left" )
{
    const OsPath root = fs::temp_directory_path() / PathFromUtf8( "bubble_project_список" );
    fs::remove_all( root );
    const ProjectFiles files = ProjectFiles::FromDirectory( root );
    for ( const string_view path : { "scripts/игрок.luau", "models/hero.glb", "models/hero.glb.import" } )
        REQUIRE( files.TryWrite( *AssetPath::TryFrom( path ), "x" ) );
    fs::create_directories( root / ".bubble/imported" );
    ofstream( root / ".bubble/imported/cache.bin" ) << "x";
    ofstream( root / ".gitignore" ) << "x";
    ofstream( root / PathFromUtf8( "models/hero.glb.saving" ) ) << "x";

    const auto listMaybe = files.TryList();
    REQUIRE( listMaybe );
    vector<string> listed;
    for ( const ListedFile& file : *listMaybe )
        listed.push_back( file.mPath.String() );
    CHECK( listed == vector<string>{ "models/hero.glb", "models/hero.glb.import", "scripts/игрок.luau" } );
    fs::remove_all( root );

    hmap<string, string> memory{ { "b.txt", "" }, { "a/c.txt", "" } };
    const auto memoryListMaybe = ProjectFiles::InMemory( memory ).TryList();
    REQUIRE( memoryListMaybe );
    CHECK( memoryListMaybe->front().mPath.View() == "a/c.txt" );
    CHECK( memoryListMaybe->back().mPath.View() == "b.txt" );
}
