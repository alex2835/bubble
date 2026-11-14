#include "bubble/editing/project_files.hpp"
#include "bubble/types/format.hpp"
#include "bubble/types/stream.hpp"

namespace bubble
{
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
        beside += ".saving";
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
    return ProjectFiles( std::move( read ), std::move( write ) );
}
}
