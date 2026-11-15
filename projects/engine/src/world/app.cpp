#include "bubble/world/app.hpp"
#include "bubble/types/format.hpp"

namespace bubble
{
App::App( AppOptions options ) : mJobs( options.mThreads )
{
}

void App::AddModule( ModuleType type )
{
    for ( const ModuleType& added : mModules )
        if ( added.mName == type.mName )
            throw logic_error( format( "App::AddModule of {} twice", type.mName ) );
    if ( type.mDescribe )
        type.mDescribe( mReflection, mComponentTypes );
    mModules.push_back( std::move( type ) );
}
}
