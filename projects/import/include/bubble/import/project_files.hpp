#pragma once
#include "bubble/core/asset_path.hpp"
#include "bubble/types/containers.hpp"
#include "bubble/types/filesystem.hpp"
#include "bubble/types/function.hpp"
#include "bubble/types/number.hpp"
#include "bubble/types/string.hpp"
#include "bubble/types/utility.hpp"

namespace bubble
{
// A file of the project as a list of them has it, with a number that changes
// whenever the file is written - its time of writing and size, on disk -
// so a look over the project tells what changed without reading it.
struct ListedFile
{
    AssetPath mPath;
    u64 mStamp = 0;

    friend bool operator==( const ListedFile&, const ListedFile& ) = default;
};

// The project's files as the editor and the importer read and write them: a
// folder on disk, or files in memory for a test. The runner never writes the
// project, so this is not the engine's.
class ProjectFiles
{
public:
    using Read = function<expected<string, string>( const AssetPath& path )>;
    using Write = function<expected<void, string>( const AssetPath& path, string_view bytes )>;
    // Every file of the project, in the order of their paths.
    using List = function<expected<vector<ListedFile>, string>()>;
    using Remove = function<expected<void, string>( const AssetPath& path )>;
    // A file to another path, the folders on the way made; one there
    // already is an error, and nothing moves.
    using Move = function<expected<void, string>( const AssetPath& from, const AssetPath& to )>;

    ProjectFiles( Read read, Write write, List list, Remove remove, Move move )
        : mRead( std::move( read ) ),
          mWrite( std::move( write ) ),
          mList( std::move( list ) ),
          mRemove( std::move( remove ) ),
          mMove( std::move( move ) )
    {
    }

    // The files under `root`. A write goes beside the file first and then
    // takes its place, so a save cut short leaves the old file whole; the
    // folders on the way are made. Folders and files whose names start with
    // a dot - .git, the import cache in .bubble - are not the project's.
    static ProjectFiles FromDirectory( OsPath root );
    // The files of `files`, by path; it outlives what this returns. As on
    // disk, what is under a folder whose name starts with a dot is read and
    // written but not listed.
    static ProjectFiles InMemory( hmap<string, string>& files );

    expected<string, string> TryRead( const AssetPath& path ) const { return mRead( path ); }
    expected<void, string> TryWrite( const AssetPath& path, string_view bytes ) const { return mWrite( path, bytes ); }
    expected<vector<ListedFile>, string> TryList() const { return mList(); }
    expected<void, string> TryRemove( const AssetPath& path ) const { return mRemove( path ); }
    expected<void, string> TryMove( const AssetPath& from, const AssetPath& to ) const { return mMove( from, to ); }

private:
    Read mRead;
    Write mWrite;
    List mList;
    Remove mRemove;
    Move mMove;
};
}
