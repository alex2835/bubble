#pragma once
#include "bubble/core/asset_path.hpp"
#include "bubble/types/filesystem.hpp"
#include "bubble/types/function.hpp"
#include "bubble/types/string.hpp"
#include "bubble/types/utility.hpp"

namespace bubble
{
// The project's files as the editor reads and writes them: a folder on disk,
// or whatever a test gives. The runner never writes the project, so this
// lives with the editor.
class ProjectFiles
{
public:
    using Read = function<expected<string, string>( const AssetPath& path )>;
    using Write = function<expected<void, string>( const AssetPath& path, string_view bytes )>;

    ProjectFiles( Read read, Write write ) : mRead( std::move( read ) ), mWrite( std::move( write ) ) {}

    // The files under `root`. A write goes beside the file first and then
    // takes its place, so a save cut short leaves the old file whole; the
    // folders on the way are made.
    static ProjectFiles FromDirectory( OsPath root );

    expected<string, string> TryRead( const AssetPath& path ) const { return mRead( path ); }
    expected<void, string> TryWrite( const AssetPath& path, string_view bytes ) const { return mWrite( path, bytes ); }

private:
    Read mRead;
    Write mWrite;
};
}
