#include "bubble/core/log.hpp"
#include "bubble/scripts/script_runtime.hpp"
#include "bubble/types/algorithm.hpp"

// Hot reload: a changed file runs again, with every script whose code
// required it, directly or through other libraries.
namespace bubble
{
expected<void, ScriptError> ScriptRuntime::TryRerun( ScriptHandle scriptHandle )
{
    // Copies held through the run: it may load and unload scripts.
    const string path = mScripts.TryGet( scriptHandle )->mPath;
    const AssetRef<ScriptAsset> assetRef = mScripts.TryGet( scriptHandle )->mAssetRef;
    auto ranMaybe = TryRunScript( path, assetRef );
    if ( not ranMaybe )
        return unexpected( std::move( ranMaybe.error() ) );
    auto scriptMaybe = mScripts.TryGet( scriptHandle );
    if ( not scriptMaybe )
        return unexpected( ScriptError{ path + " was unloaded while it ran", {} } );
    scriptMaybe->mEnvironment = std::move( ranMaybe->mEnvironment );
    scriptMaybe->mProps = std::move( ranMaybe->mProps );
    scriptMaybe->mLocals = std::move( ranMaybe->mLocals );
    scriptMaybe->mCallbacks = std::move( ranMaybe->mCallbacks );

    // Instances keep self, gain the new props and locals, and come back on.
    const LuaTable props = scriptMaybe->mProps;
    const LuaTable locals = scriptMaybe->mLocals;
    for ( const ScriptInstanceHandle instanceHandle : mInstances.Handles() )
    {
        auto instanceMaybe = mInstances.TryGet( instanceHandle );
        if ( instanceMaybe->mScriptHandle != scriptHandle )
            continue;
        // Props copy as Create copied them, a failure there would have failed
        // the run; a copy fails only on tables nested past the limit, and
        // then the instance goes on without the new value.
        (void)TryCopyInto( instanceMaybe->mSelf, props, true );
        (void)TryCopyInto( instanceMaybe->mSelf, locals, true );
        instanceMaybe->mEnabled = true;
    }
    return {};
}

bool ScriptRuntime::Requires( const LuaTable& env, const hset<string>& libraries )
{
    // A library dropped after a change has no environment until it runs
    // again, and is to run again anyway.
    if ( env.IsNil() )
        return false;
    const auto requiredMaybe = mRequiresOf[env].TryAs<LuaTable>();
    if ( not requiredMaybe )
        return false;
    return ranges::any_of( requiredMaybe->Pairs(), [&]( const auto& entry ) {
        return libraries.contains( entry.first.template TryAs<string>().value_or( "" ) );
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
    for ( const ScriptHandle scriptHandle : mScripts.Handles() )
    {
        // Another script's run may have unloaded this one.
        const auto scriptMaybe = mScripts.TryGet( scriptHandle );
        if ( not scriptMaybe or ( scriptMaybe->mPath != path and not Requires( scriptMaybe->mEnvironment, changed ) ) )
            continue;
        if ( auto rerunMaybe = TryRerun( scriptHandle ); not rerunMaybe )
            LogError( "{}\n{}", rerunMaybe.error().mMessage, rerunMaybe.error().mTraceback );
    }
}
}
