#include "bubble/core/profile.hpp"
#include "bubble/scripts/script_runtime.hpp"

// Coroutines of instances: start( fn ), wait, wait_until, and the tick
// that resumes them.
namespace bubble
{
void ScriptRuntime::Start( ScriptInstanceHandle instanceHandle, const LuaFunction& fn, const LuaRest& args )
{
    const LuaThread thread = mLua.NewThread( fn );
    mInstances.TryGet( instanceHandle )->mTasks.push_back( Task{ thread, {}, false } );
    // To its first wait, at once.
    Resume( instanceHandle, thread, args );
}

void ScriptRuntime::Resume( ScriptInstanceHandle instanceHandle, const LuaThread& thread, const LuaRest& args )
{
    // The caller's copy of the thread outlives the instance: a wait_until
    // condition may have destroyed it or switched it off just now.
    if ( not Enabled( instanceHandle ) )
        return;
    LuaResume resumed;
    {
        const CurrentScope scope( *this, instanceHandle );
        resumed = thread.Resume( args );
    }
    if ( resumed.mStatus == LuaResume::Status::Failed )
    {
        Fail( instanceHandle, resumed.mError );
        return;
    }
    // Found again by its thread: the coroutine may have started others,
    // or destroyed its own instance.
    const auto instanceMaybe = mInstances.TryGet( instanceHandle );
    if ( not instanceMaybe )
        return;
    for ( Task& task : instanceMaybe->mTasks )
        if ( task.mThread == thread )
        {
            task.mWait = resumed.mWait;
            task.mDone = resumed.mStatus == LuaResume::Status::Finished;
        }
}

void ScriptRuntime::TickInstance( ScriptInstanceHandle instanceHandle, f32 dt )
{
    if ( not Enabled( instanceHandle ) )
        return;
    // Coroutines started during this tick have run already.
    const size_t count = mInstances.TryGet( instanceHandle )->mTasks.size();
    for ( size_t i = 0; i < count and Enabled( instanceHandle ); ++i )
    {
        const auto instanceMaybe = mInstances.TryGet( instanceHandle );
        if ( i >= instanceMaybe->mTasks.size() )
            break;
        const Task task = instanceMaybe->mTasks[i];
        if ( task.mDone )
            continue;

        bool ready = true;
        if ( const auto secondsMaybe = task.mWait.TryAs<f64>() )
        {
            const f64 left = *secondsMaybe - dt;
            ready = left <= 0;
            if ( not ready )
                instanceMaybe->mTasks[i].mWait = mLua.Value( left );
        }
        else if ( const auto conditionMaybe = task.mWait.TryAs<LuaFunction>() )
        {
            expected<LuaValue, ScriptError> polledMaybe;
            {
                const CurrentScope scope( *this, instanceHandle );
                polledMaybe = ( *conditionMaybe )();
            }
            if ( not polledMaybe )
            {
                Fail( instanceHandle, polledMaybe.error() );
                break;
            }
            ready = polledMaybe->Truthy();
        }
        if ( ready )
            Resume( instanceHandle, task.mThread, {} );
    }
    if ( const auto instanceMaybe = mInstances.TryGet( instanceHandle ) )
        erase_if( instanceMaybe->mTasks, []( const Task& task ) { return task.mDone; } );
}

void ScriptRuntime::Tick( f32 dt )
{
    BUBBLE_PROFILE_ZONE();
    // An instance made during the tick waits for the next one.
    for ( const ScriptInstanceHandle instanceHandle : mInstances.Handles() )
        TickInstance( instanceHandle, dt );
}
}
