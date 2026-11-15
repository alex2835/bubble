#pragma once
#include "bubble/core/asset_path.hpp"
#include "bubble/types/containers.hpp"
#include "bubble/types/filesystem.hpp"
#include "bubble/types/function.hpp"
#include "bubble/types/string.hpp"
#include "bubble/types/utility.hpp"

namespace bubble
{
// The project's files as the editor and the importer read and write them: a
// folder on disk, or files in memory for a test. The runner never writes the
// project, so this is not the engine's.
class ProjectFiles
{
public:
    using Read = function<expected<string, string>( const AssetPath& path )>;
    using Write = function<expected<void, string>( const AssetPath& path, string_view bytes )>;
    // Every file of the project, in the order of their paths.
    using List = function<expected<vector<AssetPath>, string>()>;

    ProjectFiles( Read read, Write write, List list )
        : mRead( std::move( read ) ),
          mWrite( std::move( write ) ),
          mList( std::move( list ) )
    {
    }

    // The files under `root`. A write goes beside the file first and then
    // takes its place, so a save cut short leaves the old file whole; the
    // folders on the way are made. Folders and files whose names start with
    // a dot - .git, the import cache in .bubble - are not the project's.
    static ProjectFiles FromDirectory( OsPath root );
    // The files of `files`, by path; it outlives what this returns.
    static ProjectFiles InMemory( hmap<string, string>& files );

    expected<string, string> TryRead( const AssetPath& path ) const { return mRead( path ); }
    expected<void, string> TryWrite( const AssetPath& path, string_view bytes ) const { return mWrite( path, bytes ); }
    expected<vector<AssetPath>, string> TryList() const { return mList(); }

private:
    Read mRead;
    Write mWrite;
    List mList;
};
}
