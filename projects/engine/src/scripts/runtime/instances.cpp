#include "bubble/core/log.hpp"
#include "bubble/scripts/script_runtime.hpp"
#include "bubble/types/algorithm.hpp"
#include "bubble/types/format.hpp"
#include "text.hpp"

// Instances: self made from the props, locals and overrides, and the
// callbacks called on it.
namespace bubble
{
expected<void, string> ScriptRuntime::TryCopyInto( const LuaTable& self, const LuaTable& from, bool onlyMissing )
{
    for ( const auto& [key, value] : from.Pairs() )
    {
        if ( onlyMissing and not self.RawGet( key ).IsNil() )
            continue;
        auto copyMaybe = mLua.TryDeepCopy( value );
        if ( not copyMaybe )
            return unexpected( std::move( copyMaybe.error() ) );
        self.RawSet( key, *copyMaybe );
    }
    return {};
}

expected<ScriptInstanceHandle, string> ScriptRuntime::TryCreate( ScriptHandle scriptHandle,
                                                                 string label,
                                                                 const LuaTable& overrides )
{
    const auto scriptMaybe = mScripts.TryGet( scriptHandle );
    if ( not scriptMaybe )
        return unexpected( label + ": its script was unloaded" );
    const LuaTable props = scriptMaybe->mProps;
    const LuaTable locals = scriptMaybe->mLocals;
    const string path = scriptMaybe->mPath;

    // Each instance gets its own copy: a table among the props or locals is
    // not shared between entities.
    LuaTable self = mLua.NewTable();
    if ( auto copiedMaybe = TryCopyInto( self, props, false ); not copiedMaybe )
        return unexpected( label + ": " + copiedMaybe.error() );
    if ( auto copiedMaybe = TryCopyInto( self, locals, false ); not copiedMaybe )
        return unexpected( label + ": " + copiedMaybe.error() );
    if ( auto overriddenMaybe = TryOverride( *mScripts.TryGet( scriptHandle ), label, self, overrides );
         not overriddenMaybe )
        return unexpected( std::move( overriddenMaybe.error() ) );

    return mInstances.Add( Instance{ scriptHandle, std::move( label ), std::move( self ), {}, {}, true } );
}

expected<void, string> ScriptRuntime::TryOverride( const Script& script,
                                                   const string& label,
                                                   const LuaTable& self,
                                                   const LuaTable& overrides )
{
    for ( const auto& [key, value] : overrides.Pairs() )
    {
        const LuaValue fallback = script.mProps.RawGet( key );
        if ( not script.mLocals.RawGet( key ).IsNil() )
            return unexpected( format( "{}: {} is a local of {}: it starts the same in every instance, the scene "
                                       "cannot set it",
                                       label, key.Describe(), script.mPath ) );
        if ( fallback.IsNil() or not key.Is( LuaKind::String ) )
        {
            vector<string> names;
            for ( const auto& entry : script.mProps.Pairs() )
                names.push_back( entry.first.TryAs<string>().value_or( "?" ) );
            ranges::sort( names );
            return unexpected( format( "{}: {} is not a prop of {} (props: {})", label, key.Describe(), script.mPath,
                                       detail::Join( names ) ) );
        }
        // The engine's types are all userdata; each is a kind of its own.
        const auto kindOf = [&]( const LuaValue& of ) {
            return of.Is( LuaKind::Userdata ) ? "a " + mLua.Describe( of ) : string( detail::KindName( of.Kind() ) );
        };
        if ( kindOf( fallback ) != kindOf( value ) )
            return unexpected( format( "{}: prop {} is {} in {}, the override is {}", label, *key.TryAs<string>(),
                                       kindOf( fallback ), script.mPath, kindOf( value ) ) );
        auto copyMaybe = mLua.TryDeepCopy( value );
        if ( not copyMaybe )
            return unexpected( label + ": " + copyMaybe.error() );
        self.RawSet( key, *copyMaybe );
    }
    return {};
}

expected<void, string> ScriptRuntime::TrySetProps( ScriptInstanceHandle instanceHandle, const LuaTable& overrides )
{
    const auto instanceMaybe = mInstances.TryGet( instanceHandle );
    if ( not instanceMaybe )
        return unexpected( "the instance is gone"s );
    const auto scriptMaybe = mScripts.TryGet( instanceMaybe->mScriptHandle );
    const string label = instanceMaybe->mLabel;
    const LuaTable self = instanceMaybe->mSelf;
    // Laid on a table of their own first, so a bad override leaves self be.
    LuaTable props = mLua.NewTable();
    if ( auto copiedMaybe = TryCopyInto( props, scriptMaybe->mProps, false ); not copiedMaybe )
        return unexpected( label + ": " + copiedMaybe.error() );
    if ( auto overriddenMaybe = TryOverride( *scriptMaybe, label, props, overrides ); not overriddenMaybe )
        return overriddenMaybe;
    for ( const auto& [key, value] : props.Pairs() )
        self.RawSet( key, value );
    return {};
}

void ScriptRuntime::Destroy( ScriptInstanceHandle instanceHandle )
{
    const auto instanceMaybe = mInstances.TryGet( instanceHandle );
    if ( not instanceMaybe )
        return;
    for ( const string& event : instanceMaybe->mEvents )
        erase_if( mEvents[event],
                  [&]( const Subscriber& subscriber ) { return subscriber.mInstanceHandle == instanceHandle; } );
    // Its coroutines and self go with it; a callback of its own still
    // running keeps them on the stack until it returns.
    mInstances.Remove( instanceHandle );
}

bool ScriptRuntime::Alive( ScriptInstanceHandle instanceHandle ) const
{
    return mInstances.Alive( instanceHandle );
}

bool ScriptRuntime::Enabled( ScriptInstanceHandle instanceHandle ) const
{
    const auto instanceMaybe = mInstances.TryGet( instanceHandle );
    return instanceMaybe and instanceMaybe->mEnabled;
}

LuaTable ScriptRuntime::Self( ScriptInstanceHandle instanceHandle ) const
{
    const auto instanceMaybe = mInstances.TryGet( instanceHandle );
    return instanceMaybe ? instanceMaybe->mSelf : LuaTable();
}

void ScriptRuntime::Fail( ScriptInstanceHandle instanceHandle, const ScriptError& error )
{
    const auto instanceMaybe = mInstances.TryGet( instanceHandle );
    if ( not instanceMaybe )
        return;
    LogError( "{}: {}\n{}", instanceMaybe->mLabel, error.mMessage, error.mTraceback );
    instanceMaybe->mEnabled = false;
    // Its coroutines stop with it; a changed file starts it over.
    instanceMaybe->mTasks.clear();
}

bool ScriptRuntime::CallWith( ScriptInstanceHandle instanceHandle, u32 callback, const LuaRest& args )
{
    const auto instanceMaybe = mInstances.TryGet( instanceHandle );
    if ( not instanceMaybe )
        return false;
    if ( not instanceMaybe->mEnabled or not Has( instanceMaybe->mScriptHandle, callback ) )
        return true;
    const LuaFunction fn = mScripts.TryGet( instanceMaybe->mScriptHandle )->mCallbacks[callback];
    const LuaTable self = instanceMaybe->mSelf;

    expected<LuaValue, ScriptError> calledMaybe;
    {
        const CurrentScope scope( *this, instanceHandle );
        calledMaybe = fn( self, args );
    }
    if ( not calledMaybe )
    {
        Fail( instanceHandle, calledMaybe.error() );
        return false;
    }
    return true;
}
}
