#include "bubble/scripts/script_api.hpp"
#include "bubble/world/app.hpp"

namespace bubble
{
void ScriptApi::ExtendComponent( Name component, Extend extend, string declaration )
{
    mExtensions.push_back( Extension{ component, std::move( extend ), std::move( declaration ) } );
}

void ScriptApi::AddGlobals( Add add, string declaration )
{
    mGlobals.push_back( Globals{ std::move( add ), std::move( declaration ) } );
}

void ScriptApi::AddCallback( string name, string signature )
{
    mCallbacks.push_back( Callback{ std::move( name ), std::move( signature ) } );
}

ScriptApi CollectScriptApi( const App& app )
{
    ScriptApi api;
    for ( const ModuleType& type : app.Modules() )
        if ( type.mScripts )
            type.mScripts( api );
    return api;
}
}
