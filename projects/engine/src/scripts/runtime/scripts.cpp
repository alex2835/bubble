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
    AssetRef<ScriptAsset> asset = mAssets.Find<ScriptAsset>( *valid );

    auto ran = RunScript( valid->View(), asset );
    if ( not ran )
        return unexpected( std::move( ran.error() ) );
    const ScriptHandle handle =
        mScripts.Add( Script{ valid->String(), std::move( asset ), std::move( ran->mEnvironment ),
                              std::move( ran->mProps ), std::move( ran->mLocals ), std::move( ran->mCallbacks ) } );
    mScriptsByPath[valid->String()] = handle;
    return handle;
}

void ScriptRuntime::Unload( ScriptHandle handle )
{
    if ( const auto script = mScripts.Get( handle ) )
        mScriptsByPath.erase( script->mChunk );
    for ( const ScriptInstanceHandle instance : mInstances.Handles() )
        if ( mInstances.Get( instance )->mScript == handle )
            Destroy( instance );
    mScripts.Remove( handle );
}

bool ScriptRuntime::Has( ScriptHandle handle, u32 callback ) const
{
    const auto script = mScripts.Get( handle );
    return script and callback < script->mCallbacks.size() and not script->mCallbacks[callback].IsNil();
}

LuaTable ScriptRuntime::Props( ScriptHandle handle ) const
{
    const auto script = mScripts.Get( handle );
    return script ? script->mProps : LuaTable();
}
}
