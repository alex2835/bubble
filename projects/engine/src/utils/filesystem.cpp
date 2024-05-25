#include "engine/utils/filesystem.hpp"

namespace bubble
{
string filesystem::readFile( const path& file )
{
    std::ifstream stream( file );
    return string( std::istreambuf_iterator<char>( stream ),
                   std::istreambuf_iterator<char>() );
}

}
