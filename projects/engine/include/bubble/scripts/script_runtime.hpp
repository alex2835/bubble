#pragma once
#include "bubble/scripts/lua/lua_call.hpp"
#include "bubble/scripts/lua/lua_ref.hpp"
#include "bubble/types/containers.hpp"
#include "bubble/types/handle.hpp"
#include "bubble/types/number.hpp"

struct lua_State;

namespace bubble
{
class LuaState;

// A script file run once in a world; its functions are shared by every
// instance and the id stays the same across reloads.
using ScriptModuleId = Handle<struct ScriptModuleTag>;
// One entity's run of a module: its self table - the props with the
// entity's overrides, and whatever the script keeps there - with its
// coroutines and subscriptions, which go when it goes.
using ScriptInstanceId = Handle<struct ScriptInstanceTag>;

// The scripts of one World: the functions every script file sees, the
// files loaded and the instances alive. It owns them all; the rest of the
// engine holds ids, and an id whose instance is gone finds nothing rather
// than something else. Built on a LuaState before it is sealed. Globals:
//   props { name = default, ... }   the file's properties, declared once
//   prop( default, info )           a property with hints for the inspector
//   start( fn, ... )                runs fn as a coroutine of the instance
//   wait( seconds ), wait_until( fn )   inside start: sleep until then
//   on( event, fn ), emit( event, ... ) events between instances
class ScriptRuntime
{
public:
    // `callbacks` names the entry points the engine calls - "on_start",
    // "on_update" - in the order Call takes them by index.
    ScriptRuntime( LuaState& state, vector<string> callbacks );
    ~ScriptRuntime();
    ScriptRuntime( const ScriptRuntime& ) = delete;
    ScriptRuntime& operator=( const ScriptRuntime& ) = delete;

    LuaState& State() const { return mState; }
    lua_State* L() const;
    const vector<string>& Callbacks() const { return mCallbacks; }

    // Runs the file once: its own environment, its props, its callbacks.
    // The state must be sealed.
    expected<ScriptModuleId, ScriptError> Load( string_view chunk, string_view bytecode );
    // Runs the changed file again in place of the old one. Instances keep
    // self, get the props the file gained, call the new functions from now
    // on, and come back on if an error had switched them off. On failure
    // the old file stays.
    expected<void, ScriptError> Reload( ScriptModuleId module, string_view bytecode );
    // Destroys the module's instances with it.
    void Unload( ScriptModuleId module );
    bool Has( ScriptModuleId module, u32 callback ) const;
    // Pushes the props' defaults as a table; nil for a module that is gone.
    void PushProps( ScriptModuleId module ) const;

    // `label` names the instance in errors - the entity's path.
    // `overrides`, when given, is the stack slot of a table laid over the
    // defaults; a key that is not a prop, or a value of another type than
    // the default, is an error.
    expected<ScriptInstanceId, string> Create( ScriptModuleId module, string label, int overrides = 0 );
    // Takes it off its events and stops its coroutines. Safe from inside
    // its own callback, and on an id that is gone already.
    void Destroy( ScriptInstanceId instance );
    bool Alive( ScriptInstanceId instance ) const;
    bool Enabled( ScriptInstanceId instance ) const;
    // Pushes self; nil for an instance that is gone.
    void PushSelf( ScriptInstanceId instance ) const;

    // Calls the callback with self and the `nargs` values on top of the
    // stack, which are popped. A file without that callback, or an
    // instance switched off, does nothing. An error is logged with the
    // label and switches the instance off until its file changes.
    expected<void, ScriptError> Call( ScriptInstanceId instance, u32 callback, int nargs = 0 );

    // Resumes the coroutines whose wait is over, instance by instance in a
    // fixed order. A paused world does not call it.
    void Tick( f32 dt );

    // Calls every subscriber of `event` with the `nargs` values on top of
    // the stack, which are popped - in the order they subscribed.
    void Emit( string_view event, int nargs );

private:
    struct Impl;
    friend struct Impl;

    struct Module
    {
        string mChunk;
        LuaRef mEnvironment;
        LuaRef mProps;
        vector<LuaRef> mCallbacks;
    };

    struct Instance
    {
        ScriptModuleId mModule;
        string mLabel;
        LuaRef mSelf;
        // Array of coroutines: { thread, wait }, wait being the seconds
        // left or a function to poll. A finished one has false for its
        // thread until the tick sweeps it out.
        LuaRef mTasks;
        // The events it is on, to take itself off them when it goes.
        str_hset mEvents;
        bool mEnabled = true;
    };

    LuaState& mState;
    vector<string> mCallbacks;
    SlotMap<Module, ScriptModuleTag> mModules;
    SlotMap<Instance, ScriptInstanceTag> mInstances;
    // event -> array of { index, generation, fn }: the subscriber's id and
    // its function, in the order of subscription.
    LuaRef mEvents;
    // Whose callback or coroutine is running: what start and on act on.
    ScriptInstanceId mCurrent;
};
}
