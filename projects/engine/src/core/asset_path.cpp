#include "bubble/core/asset_path.hpp"
#include "bubble/core/utf8.hpp"
#include "bubble/types/containers.hpp"
#include "bubble/types/format.hpp"

namespace bubble
{
expected<AssetPath, string> AssetPath::From( string_view text )
{
    if ( not Utf8Valid( text ) )
        return unexpected( "the path is not valid UTF-8" );
    if ( text.starts_with( '/' ) or text.starts_with( '\\' ) or ( text.size() > 1 and text[1] == ':' ) )
        return unexpected( format( "'{}' is absolute; asset paths start at the project's root", text ) );

    vector<string_view> steps;
    size_t start = 0;
    while ( start <= text.size() )
    {
        const size_t end = text.find_first_of( "/\\", start );
        const string_view step = text.substr( start, end == string_view::npos ? string_view::npos : end - start );
        if ( step == ".." )
            return unexpected( format( "'{}' leaves the project with '..'", text ) );
        if ( not step.empty() and step != "." )
            steps.push_back( step );
        if ( end == string_view::npos )
            break;
        start = end + 1;
    }

    string path;
    for ( const string_view step : steps )
    {
        if ( not path.empty() )
            path += '/';
        path += step;
    }
    return AssetPath( std::move( path ) );
}

string_view AssetPath::Filename() const
{
    const size_t slash = mPath.rfind( '/' );
    return slash == string::npos ? string_view( mPath ) : string_view( mPath ).substr( slash + 1 );
}

string_view AssetPath::Extension() const
{
    const string_view name = Filename();
    const size_t dot = name.rfind( '.' );
    // ".gitignore" is a name, not an extension.
    return dot == string_view::npos or dot == 0 ? string_view() : name.substr( dot );
}

string_view AssetPath::Stem() const
{
    const string_view name = Filename();
    return name.substr( 0, name.size() - Extension().size() );
}

AssetPath AssetPath::Parent() const
{
    const size_t slash = mPath.rfind( '/' );
    return slash == string::npos ? AssetPath() : AssetPath( mPath.substr( 0, slash ) );
}

expected<AssetPath, string> AssetPath::Join( string_view relative ) const
{
    if ( mPath.empty() )
        return From( relative );
    return From( format( "{}/{}", mPath, relative ) );
}

OsPath AssetPath::ToOsPath( const OsPath& projectRoot ) const
{
    return projectRoot / PathFromUtf8( mPath );
}
}
