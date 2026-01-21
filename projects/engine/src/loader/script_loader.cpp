#include "engine/pch/pch.hpp"
#include "engine/loader/loader.hpp"

namespace bubble
{
Ref<Script> LoadScript( const path& scriptPath )
{
    if ( not filesystem::exists( scriptPath ) )
        return nullptr;

    return CreateRef<Script>(
        Script{ 
            .mPath = scriptPath,
            .mName = scriptPath.stem().string(),
            .mCode = filesystem::readFile( scriptPath )
        }
    );
}

Ref<Script> Loader::LoadScript( const path& scriptPath )
{
    auto [relPath, absPath] = RelAbsFromProjectPath( scriptPath );

    auto iter = mScripts.find( relPath );
    if ( iter != mScripts.end() )
        return iter->second;

    auto script = bubble::LoadScript( absPath );
    if ( not script )
    {
        LogError( "Failed to load script" );
        return nullptr;
    }

    mScripts.emplace( relPath, script );
    return script;
}

}