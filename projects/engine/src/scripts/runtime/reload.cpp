#include "bubble/core/log.hpp"
#include "bubble/scripts/script_runtime.hpp"
#include "bubble/types/algorithm.hpp"

// Hot reload: a changed file runs again, with every script whose code
// required it, directly or through other libraries.
namespace bubble
{
expected<void, ScriptError> ScriptRuntime::Rerun( ScriptHandle handle )
{
    // Copies held through the run: it may load and unload scripts.
    const string chunk = mScripts.Get( handle )->mChunk;
    const AssetRef<ScriptAsset> asset = mScripts.Get( handle )->mAsset;
    auto ran = RunScript( chunk, asset );
    if ( not ran )
        return unexpected( std::move( ran.error() ) );
    auto script = mScripts.Get( handle );
    if ( not script )
        return unexpected( ScriptError{ chunk + " was unloaded while it ran", {} } );
    script->mEnvironment = std::move( ran->mEnvironment );
    script->mProps = std::move( ran->mProps );
    script->mLocals = std::move( ran->mLocals );
    script->mCallbacks = std::move( ran->mCallbacks );

    // Instances keep self, gain the new props and locals, and come back on.
    const LuaTable props = script->mProps;
    const LuaTable locals = script->mLocals;
    for ( const ScriptInstanceHandle instanceHandle : mInstances.Handles() )
    {
        auto instance = mInstances.Get( instanceHandle );
        if ( instance->mScript != handle )
            continue;
        // Props copy as Create copied them, a failure there would have failed
        // the run; a copy fails only on tables nested past the limit, and
        // then the instance goes on without the new value.
        (void)CopyInto( instance->mSelf, props, true );
        (void)CopyInto( instance->mSelf, locals, true );
        instance->mEnabled = true;
    }
    return {};
}

bool ScriptRuntime::Requires( const LuaTable& env, const hset<string>& libraries )
{
    // A library dropped after a change has no environment until it runs
    // again, and is to run again anyway.
    if ( env.IsNil() )
        return false;
    const auto required = mRequiresOf[env].As<LuaTable>();
    if ( not required )
        return false;
    return ranges::any_of( required->Pairs(), [&]( const auto& entry ) {
        return libraries.contains( entry.first.template As<string>().value_or( "" ) );
    } );
}

void ScriptRuntime::Changed( const AssetEntryBase& entry )
{
    const string& path = entry.Path().String();
    // Everything that holds a value of the old file: the library itself,
    // the libraries whose code required it, theirs in turn.
    hset<string> changed;
    if ( mLibraries.contains( path ) )
        changed.insert( path );
    for ( bool grew = not changed.empty(); grew; )
    {
        grew = false;
        for ( const auto& [file, library] : mLibraries )
            if ( not changed.contains( file ) and Requires( library.mEnvironment, changed ) )
                grew = changed.insert( file ).second;
    }
    for ( const string& library : changed )
    {
        Library& dropped = mLibraries.find( library )->second;
        dropped.mResult = {};
        dropped.mEnvironment = {};
    }

    // Then the scripts: the changed one, and those that required any of
    // the changed libraries. A failure is logged and leaves the old code.
    for ( const ScriptHandle handle : mScripts.Handles() )
    {
        // Another script's run may have unloaded this one.
        const auto script = mScripts.Get( handle );
        if ( not script or ( script->mChunk != path and not Requires( script->mEnvironment, changed ) ) )
            continue;
        if ( auto rerun = Rerun( handle ); not rerun )
            LogError( "{}\n{}", rerun.error().mMessage, rerun.error().mTraceback );
    }
}
}
