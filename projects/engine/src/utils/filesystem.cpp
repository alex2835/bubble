#include "engine/utils/filesystem.hpp"

namespace bubble::filesystem
{
string readFile( const path& file )
{
    std::ifstream stream( file );
    if ( not stream.is_open() )
        throw std::runtime_error( "Failed to open file: " + file.string() );

    return string( std::istreambuf_iterator<char>( stream ),
                   std::istreambuf_iterator<char>() );
}

std::ifstream openStream( const path& file )
{
    std::ifstream stream( file );
    if ( not stream.is_open() )
        throw std::runtime_error( "Failed to open file: " + file.string() );
    return stream;
}

}
