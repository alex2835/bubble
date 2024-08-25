#pragma once
#include "engine/loader/loader.hpp"

namespace bubble
{
Ref<Script> Loader::LoadScript( const path& scriptPath )
{
    auto [relPath, absPath] = RelAbsFromProjectPath( scriptPath );

    auto iter = mScripts.find( relPath );
    if ( iter != mScripts.end() )
        return iter->second;

    auto script = CreateRef<Script>( absPath );
    mScripts.emplace( relPath, script );
    return script;
}

}