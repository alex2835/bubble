// The project's files on disk as the editor writes them: whole or not at
// all, folders made on the way, names in any script.
#include "bubble/core/utf8.hpp"
#include "bubble/editing/project_files.hpp"
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
