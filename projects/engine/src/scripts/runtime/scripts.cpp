#include "bubble/core/asset_path.hpp"
#include "bubble/scripts/script_runtime.hpp"
#include "bubble/types/format.hpp"

// Entity scripts: loaded once per path, unloaded with their instances.
namespace bubble
{
expected<ScriptHandle, ScriptError> ScriptRuntime::TryLoad( string_view path )
{
    if ( const auto known = mScriptsByPath.find( path ); known != mScriptsByPath.end() )
        return known->second;
    auto validMaybe = AssetPath::TryFrom( path );
    if ( not validMaybe )
        return unexpected( ScriptError{ format( "{}: {}", path, validMaybe.error() ), {} } );
    AssetRef<ScriptAsset> assetRef = mAssets.TryFind<ScriptAsset>( *validMaybe );

    auto ranMaybe = TryRunScript( validMaybe->View(), assetRef );
    if ( not ranMaybe )
        return unexpected( std::move( ranMaybe.error() ) );
    const ScriptHandle scriptHandle = mScripts.Add(
        Script{ validMaybe->String(), std::move( assetRef ), std::move( ranMaybe->mEnvironment ),
                std::move( ranMaybe->mProps ), std::move( ranMaybe->mLocals ), std::move( ranMaybe->mCallbacks ) } );
    mScriptsByPath[validMaybe->String()] = scriptHandle;
    return scriptHandle;
}

void ScriptRuntime::Unload( ScriptHandle scriptHandle )
{
    if ( const auto scriptMaybe = mScripts.TryGet( scriptHandle ) )
        mScriptsByPath.erase( scriptMaybe->mPath );
    for ( const ScriptInstanceHandle instanceHandle : mInstances.Handles() )
        if ( mInstances.TryGet( instanceHandle )->mScriptHandle == scriptHandle )
            Destroy( instanceHandle );
    mScripts.Remove( scriptHandle );
}

bool ScriptRuntime::Has( ScriptHandle scriptHandle, u32 callback ) const
{
    const auto scriptMaybe = mScripts.TryGet( scriptHandle );
    return scriptMaybe and callback < scriptMaybe->mCallbacks.size() and not scriptMaybe->mCallbacks[callback].IsNil();
}

LuaTable ScriptRuntime::Props( ScriptHandle scriptHandle ) const
{
    const auto scriptMaybe = mScripts.TryGet( scriptHandle );
    return scriptMaybe ? scriptMaybe->mProps : LuaTable();
}
}
