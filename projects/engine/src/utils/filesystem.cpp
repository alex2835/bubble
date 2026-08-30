#include "engine/utils/filesystem.hpp"

#if defined(_WIN32)
#   include <windows.h>
#endif

namespace bubble::filesystem
{
static path findExecutableDir()
{
    std::error_code error;
#if defined(_WIN32)
    // MAX_PATH is not enough for long paths, grow until the name fits.
    std::wstring buffer( MAX_PATH, 0 );
    while ( true )
    {
        auto written = GetModuleFileNameW( nullptr, buffer.data(), (DWORD)buffer.size() );
        if ( written == 0 )
            return current_path( error );

        if ( written < buffer.size() )
        {
            buffer.resize( written );
            break;
        }
        buffer.resize( buffer.size() * 2 );
    }
    return path( buffer ).parent_path();
#elif defined(__EMSCRIPTEN__)
    return current_path( error );
#else
    auto executable = read_symlink( "/proc/self/exe", error );
    if ( error )
        return current_path( error );
    return executable.parent_path();
#endif
}

const path& executableDir()
{
    static const path dir = findExecutableDir();
    return dir;
}

path resolveNearExecutable( const path& relative )
{
    if ( relative.is_absolute() )
        return relative;
    return executableDir() / relative;
}

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
