#pragma once
#include "bubble/assets/asset_registry.hpp"
#include "bubble/scripts/lua/lua_call.hpp"
#include "bubble/scripts/lua/lua_ref.hpp"
#include "bubble/scripts/script_asset.hpp"
#include "bubble/types/containers.hpp"
#include "bubble/types/handle.hpp"
#include "bubble/types/number.hpp"

struct lua_State;

namespace bubble
{
class LuaState;

// A script file run once in a world; its functions are shared by every
// instance and the handle stays the same across reloads.
using ScriptModuleHandle = Handle<struct ScriptModuleTag>;
// One entity's run of a module: its self table - the props with the
// entity's overrides, and whatever the script keeps there - with its
// coroutines and subscriptions, which go when it goes.
using ScriptInstanceHandle = Handle<struct ScriptInstanceTag>;

// The scripts of one World: the functions every script file sees, the
// files loaded and the instances alive. It owns them all; the rest of the
// engine holds handles, and a handle whose instance is gone finds nothing rather
// than something else. Built on a LuaState before it is sealed. Globals:
//   props { name = default, ... }   the file's properties, declared once
//   prop( default, info )           a property with hints for the inspector
//   require( "./x" | "@alias/x" )   a library's value, shared by the world
//   start( fn, ... )                runs fn as a coroutine of the instance
//   wait( seconds ), wait_until( fn )   inside start: sleep until then
//   on( event, fn ), emit( event, ... ) events between instances
//
// Two kinds of file: an entity script (props, callbacks, run by Load) and a
// library (returns what it shares, run by its first require). Paths follow
// Luau's own require, so luau-lsp resolves them the same way: relative to
// the requiring file, or through an alias of the project's .luaurc; no
// extension, no bare paths.
//
// The code comes from the asset registry: the runtime holds handles to the
// script assets it ran and nothing else of them. It never loads: a file a
// world needs is loaded before its scripts run. When the registry reloads a
// script, the runtime runs it again, with everything that required it.
class ScriptRuntime
{
public:
    // `callbacks` names the entry points the engine calls - "on_start",
    // "on_update" - in the order Call takes them by index. `assets` outlives
    // the runtime.
    ScriptRuntime( LuaState& state, AssetRegistry& assets, vector<string> callbacks );
    ~ScriptRuntime();
    ScriptRuntime( const ScriptRuntime& ) = delete;
    ScriptRuntime& operator=( const ScriptRuntime& ) = delete;

    LuaState& State() const { return mState; }
    lua_State* L() const;
    const vector<string>& Callbacks() const { return mCallbacks; }

    // `@name/...` reads from `directory`, a path from the project's root;
    // what .luaurc's aliases say.
    expected<void, string> SetAlias( string_view name, string_view directory );
    // Runs the script at `path` - a path from the project's root, what its
    // relative requires start from - once: its own environment, its props,
    // its callbacks. The same path again gives the same module. The asset
    // must be loaded in the registry and the state sealed.
    expected<ScriptModuleHandle, ScriptError> Load( string_view path );
    // Destroys the module's instances with it.
    void Unload( ScriptModuleHandle module );
    bool Has( ScriptModuleHandle module, u32 callback ) const;
    // Pushes the props' defaults as a table; nil for a module that is gone.
    void PushProps( ScriptModuleHandle module ) const;

    // `label` names the instance in errors - the entity's path.
    // `overrides`, when given, is the stack slot of a table laid over the
    // defaults; a key that is not a prop, or a value of another type than
    // the default, is an error.
    expected<ScriptInstanceHandle, string> Create( ScriptModuleHandle module, string label, int overrides = 0 );
    // Takes it off its events and stops its coroutines. Safe from inside
    // its own callback, and on a handle that is gone already.
    void Destroy( ScriptInstanceHandle instance );
    bool Alive( ScriptInstanceHandle instance ) const;
    bool Enabled( ScriptInstanceHandle instance ) const;
    // Pushes self; nil for an instance that is gone.
    void PushSelf( ScriptInstanceHandle instance ) const;

    // Calls the callback with self and the `nargs` values on top of the
    // stack, which are popped. A file without that callback, or an
    // instance switched off, does nothing. An error is logged with the
    // label and switches the instance off until its file changes.
    expected<void, ScriptError> Call( ScriptInstanceHandle instance, u32 callback, int nargs = 0 );

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
        AssetHandle<ScriptAsset> mAsset;
        LuaRef mEnvironment;
        LuaRef mProps;
        vector<LuaRef> mCallbacks;
    };

    struct Instance
    {
        ScriptModuleHandle mModule;
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

    struct Library
    {
        AssetHandle<ScriptAsset> mAsset;
        // Empty until its first require, and again after it changes.
        LuaRef mResult;
        LuaRef mEnvironment;
    };

    // A script asset changed: it runs again, and so does everything that
    // required it - libraries at their next require, scripts at once.
    void Changed( const AssetSlotBase& slot );
    expected<void, ScriptError> Rerun( ScriptModuleHandle module );

    LuaState& mState;
    AssetRegistry& mAssets;
    AssetListenerHandle mListener;
    vector<string> mCallbacks;
    str_hmap<string> mAliases;
    str_hmap<Library> mLibraries;
    // File -> the libraries it required: who to reload when one changes.
    str_hmap<str_hset> mRequires;
    // The files running now, outermost first: a library met again is a
    // require cycle.
    vector<string> mRunning;
    SlotMap<Module, ScriptModuleTag> mModules;
    str_hmap<ScriptModuleHandle> mModulesByPath;
    SlotMap<Instance, ScriptInstanceTag> mInstances;
    // event -> array of { index, generation, fn }: the subscriber's handle and
    // its function, in the order of subscription.
    LuaRef mEvents;
    // Whose callback or coroutine is running: what start and on act on.
    ScriptInstanceHandle mCurrent;
};
}
