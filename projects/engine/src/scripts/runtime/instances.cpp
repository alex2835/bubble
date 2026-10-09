#include "bubble/core/log.hpp"
#include "bubble/scripts/script_runtime.hpp"
#include "bubble/types/algorithm.hpp"
#include "bubble/types/format.hpp"
#include "text.hpp"

// Instances: self made from the props, locals and overrides, and the
// callbacks called on it.
namespace bubble
{
expected<void, string> ScriptRuntime::CopyInto( const LuaTable& self, const LuaTable& from, bool onlyMissing )
{
    for ( const auto& [key, value] : from.Pairs() )
    {
        if ( onlyMissing and not self.RawGet( key ).IsNil() )
            continue;
        auto copy = mLua.DeepCopy( value );
        if ( not copy )
            return unexpected( std::move( copy.error() ) );
        self.RawSet( key, *copy );
    }
    return {};
}

expected<ScriptInstanceHandle, string> ScriptRuntime::Create( ScriptHandle scriptHandle,
                                                              string label,
                                                              const LuaTable& overrides )
{
    const auto script = mScripts.Get( scriptHandle );
    if ( not script )
        return unexpected( label + ": its script was unloaded" );
    const LuaTable props = script->mProps;
    const LuaTable locals = script->mLocals;
    const string path = script->mPath;

    // Each instance gets its own copy: a table among the props or locals is
    // not shared between entities.
    LuaTable self = mLua.NewTable();
    if ( auto copied = CopyInto( self, props, false ); not copied )
        return unexpected( label + ": " + copied.error() );
    if ( auto copied = CopyInto( self, locals, false ); not copied )
        return unexpected( label + ": " + copied.error() );

    for ( const auto& [key, value] : overrides.Pairs() )
    {
        const LuaValue fallback = props.RawGet( key );
        if ( not locals.RawGet( key ).IsNil() )
            return unexpected( format( "{}: {} is a local of {}: it starts the same in every instance, the scene "
                                       "cannot set it",
                                       label, key.Describe(), path ) );
        if ( fallback.IsNil() or not key.Is( LuaKind::String ) )
        {
            vector<string> names;
            for ( const auto& entry : props.Pairs() )
                names.push_back( entry.first.As<string>().value_or( "?" ) );
            ranges::sort( names );
            return unexpected( format( "{}: {} is not a prop of {} (props: {})", label, key.Describe(), path,
                                       detail::Join( names ) ) );
        }
        if ( fallback.Kind() != value.Kind() )
            return unexpected( format( "{}: prop {} is {} in {}, the override is {}", label, *key.As<string>(),
                                       detail::KindName( fallback.Kind() ), path, detail::KindName( value.Kind() ) ) );
        auto copy = mLua.DeepCopy( value );
        if ( not copy )
            return unexpected( label + ": " + copy.error() );
        self.RawSet( key, *copy );
    }

    return mInstances.Add( Instance{ scriptHandle, std::move( label ), std::move( self ), {}, {}, true } );
}

void ScriptRuntime::Destroy( ScriptInstanceHandle instanceHandle )
{
    const auto instance = mInstances.Get( instanceHandle );
    if ( not instance )
        return;
    for ( const string& event : instance->mEvents )
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
    const auto instance = mInstances.Get( instanceHandle );
    return instance and instance->mEnabled;
}

LuaTable ScriptRuntime::Self( ScriptInstanceHandle instanceHandle ) const
{
    const auto instance = mInstances.Get( instanceHandle );
    return instance ? instance->mSelf : LuaTable();
}

void ScriptRuntime::Fail( ScriptInstanceHandle instanceHandle, const ScriptError& error )
{
    const auto instance = mInstances.Get( instanceHandle );
    if ( not instance )
        return;
    LogError( "{}: {}\n{}", instance->mLabel, error.mMessage, error.mTraceback );
    instance->mEnabled = false;
    // Its coroutines stop with it; a changed file starts it over.
    instance->mTasks.clear();
}

bool ScriptRuntime::CallWith( ScriptInstanceHandle instanceHandle, u32 callback, const LuaRest& args )
{
    const auto instance = mInstances.Get( instanceHandle );
    if ( not instance )
        return false;
    if ( not instance->mEnabled or not Has( instance->mScriptHandle, callback ) )
        return true;
    const LuaFunction fn = mScripts.Get( instance->mScriptHandle )->mCallbacks[callback];
    const LuaTable self = instance->mSelf;

    expected<LuaValue, ScriptError> called;
    {
        const CurrentScope scope( *this, instanceHandle );
        called = fn( self, args );
    }
    if ( not called )
    {
        Fail( instanceHandle, called.error() );
        return false;
    }
    return true;
}
}
