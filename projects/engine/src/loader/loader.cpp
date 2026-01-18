#include "engine/loader/loader.hpp"

namespace bubble
{
Loader::Loader()
    :  mErrorModel( LoadModel( ERROR_MODEL ) ),
       mErrorTexture( LoadTexture2D( ERROR_TEXTURE ) )
{}

Loader::ProjectPath Loader::RelAbsFromProjectPath( const path& resourcePath ) const
{
    // engine internal resource
    if ( resourcePath.string().starts_with( "." ) )
        return { resourcePath, resourcePath };

    auto relPath = resourcePath.is_relative() ? resourcePath : filesystem::relative( resourcePath, mProjectRootPath );
    auto absPath = resourcePath.is_absolute() ? resourcePath : mProjectRootPath / resourcePath;
    return ProjectPath{ .rel = relPath.generic_string(),
                        .abs = absPath.generic_string() };
}


} // namespace bubble