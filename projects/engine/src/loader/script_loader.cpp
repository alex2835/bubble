#include "engine/loader/loader.hpp"

namespace bubble
{
Ref<Script> LoadScript( const path& scriptPath )
{
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
    mScripts.emplace( relPath, script );
    return script;
}

}