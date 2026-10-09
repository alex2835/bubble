#include "bubble/core/asset_path.hpp"
#include "bubble/scripts/script_runtime.hpp"
#include "bubble/types/format.hpp"

// Entity scripts: loaded once per path, unloaded with their instances.
namespace bubble
{
expected<ScriptHandle, ScriptError> ScriptRuntime::Load( string_view path )
{
    if ( const auto known = mScriptsByPath.find( path ); known != mScriptsByPath.end() )
        return known->second;
    auto valid = AssetPath::From( path );
    if ( not valid )
        return unexpected( ScriptError{ format( "{}: {}", path, valid.error() ), {} } );
    AssetRef<ScriptAsset> assetRef = mAssets.Find<ScriptAsset>( *valid );

    auto ran = RunScript( valid->View(), assetRef );
    if ( not ran )
        return unexpected( std::move( ran.error() ) );
    const ScriptHandle scriptHandle =
        mScripts.Add( Script{ valid->String(), std::move( assetRef ), std::move( ran->mEnvironment ),
                              std::move( ran->mProps ), std::move( ran->mLocals ), std::move( ran->mCallbacks ) } );
    mScriptsByPath[valid->String()] = scriptHandle;
    return scriptHandle;
}

void ScriptRuntime::Unload( ScriptHandle scriptHandle )
{
    if ( const auto script = mScripts.Get( scriptHandle ) )
        mScriptsByPath.erase( script->mPath );
    for ( const ScriptInstanceHandle instanceHandle : mInstances.Handles() )
        if ( mInstances.Get( instanceHandle )->mScriptHandle == scriptHandle )
            Destroy( instanceHandle );
    mScripts.Remove( scriptHandle );
}

bool ScriptRuntime::Has( ScriptHandle scriptHandle, u32 callback ) const
{
    const auto script = mScripts.Get( scriptHandle );
    return script and callback < script->mCallbacks.size() and not script->mCallbacks[callback].IsNil();
}

LuaTable ScriptRuntime::Props( ScriptHandle scriptHandle ) const
{
    const auto script = mScripts.Get( scriptHandle );
    return script ? script->mProps : LuaTable();
}
}
