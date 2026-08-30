#include <atomic>
#include "engine/loader/loader.hpp"

namespace bubble
{
u64 Loader::NextResourcesGeneration()
{
    static std::atomic<u64> counter = 0;
    return ++counter;
}


Loader::ProjectPath Loader::RelAbsFromProjectPath( const path& resourcePath ) const
{
    // engine internal resource
    if ( resourcePath.string().starts_with( "." ) )
        return { resourcePath, resourcePath };

    auto relPath = resourcePath.is_relative() ? resourcePath : filesystem::relative( resourcePath, mProjectRootDir );
    auto absPath = resourcePath.is_absolute() ? resourcePath : mProjectRootDir / resourcePath;
    return ProjectPath{ .rel = relPath.generic_string(),
                        .abs = absPath.generic_string() };
}


} // namespace bubble