#include "bubble/import/project_files.hpp"
#include "bubble/core/utf8.hpp"
#include "bubble/types/algorithm.hpp"
#include "bubble/types/format.hpp"
#include "bubble/types/stream.hpp"

namespace bubble
{
namespace
{
constexpr string_view cSaving = ".saving";
}

ProjectFiles ProjectFiles::FromDirectory( OsPath root )
{
    Read read = [root]( const AssetPath& path ) -> expected<string, string> {
        ifstream file( path.ToOsPath( root ), ios::binary );
        if ( not file )
            return unexpected( "no such file"s );
        ostringstream bytes;
        bytes << file.rdbuf();
        return std::move( bytes ).str();
    };
    Write write = [root]( const AssetPath& path, string_view bytes ) -> expected<void, string> {
        const OsPath target = path.ToOsPath( root );
        OsPath beside = target;
        beside += PathFromUtf8( cSaving );
        error_code error;
        fs::create_directories( target.parent_path(), error );
        if ( error )
            return unexpected( format( "cannot make its folder: {}", error.message() ) );
        {
            ofstream file( beside, ios::binary | ios::trunc );
            if ( not file )
                return unexpected( "cannot write it"s );
            file.write( bytes.data(), static_cast<streamsize>( bytes.size() ) );
            file.close();
            if ( not file )
                return unexpected( "cannot write it to the end"s );
        }
        fs::rename( beside, target, error );
        if ( error )
        {
            const string why = error.message();
            fs::remove( beside, error );
            return unexpected( format( "cannot put it in place: {}", why ) );
        }
        return {};
    };
    List list = [root]() -> expected<vector<AssetPath>, string> {
        vector<AssetPath> paths;
        error_code error;
        fs::recursive_directory_iterator walk( root, error );
        for ( ; not error and walk != fs::recursive_directory_iterator(); walk.increment( error ) )
        {
            const string name = PathToUtf8( walk->path().filename() );
            if ( name.starts_with( "." ) )
            {
                walk.disable_recursion_pending();
                continue;
            }
            // What a write cut short left beside its file.
            if ( not walk->is_regular_file() or name.ends_with( cSaving ) )
                continue;
            auto pathMaybe = AssetPath::TryFrom( PathToUtf8( walk->path().lexically_relative( root ) ) );
            if ( not pathMaybe )
                return unexpected( std::move( pathMaybe.error() ) );
            paths.push_back( std::move( *pathMaybe ) );
        }
        if ( error )
            return unexpected( format( "cannot list the project: {}", error.message() ) );
        ranges::sort( paths );
        return paths;
    };
    return ProjectFiles( std::move( read ), std::move( write ), std::move( list ) );
}

ProjectFiles ProjectFiles::InMemory( hmap<string, string>& files )
{
    Read read = [&files]( const AssetPath& path ) -> expected<string, string> {
        const auto found = files.find( path.View() );
        if ( found == files.end() )
            return unexpected( "no such file"s );
        return found->second;
    };
    Write write = [&files]( const AssetPath& path, string_view bytes ) -> expected<void, string> {
        files.insert_or_assign( path.String(), string( bytes ) );
        return {};
    };
    List list = [&files]() -> expected<vector<AssetPath>, string> {
        vector<AssetPath> paths;
        for ( const auto& [path, bytes] : files )
            // As on disk, what is under a folder whose name starts with a dot
            // is not the project's.
            if ( not path.starts_with( '.' ) and path.find( "/." ) == string::npos )
                paths.push_back( *AssetPath::TryFrom( path ) );
        ranges::sort( paths );
        return paths;
    };
    return ProjectFiles( std::move( read ), std::move( write ), std::move( list ) );
}
}
