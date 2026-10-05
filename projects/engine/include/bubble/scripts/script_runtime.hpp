#pragma once
#include "bubble/assets/asset_registry.hpp"
#include "bubble/scripts/lua.hpp"
#include "bubble/scripts/script_asset.hpp"
#include "bubble/types/containers.hpp"
#include "bubble/types/handle.hpp"
#include "bubble/types/number.hpp"

namespace bubble
{
// A script file run once in a world; its functions are shared by every
// instance and the handle stays the same across reloads.
using ScriptModuleHandle = Handle<struct ScriptModuleTag>;
// One entity's run of a module: its self table - the props with the
// entity's overrides, and whatever the script keeps there - with its
// coroutines and subscriptions, which go when it goes.
using ScriptInstanceHandle = Handle<struct ScriptInstanceTag>;

// The scripts of one World: the functions every script file sees, the
// files run and the instances alive. It owns them all; the rest of the
// engine holds handles, and a handle whose instance is gone finds nothing
// rather than something else. Built on a LuaState before it is sealed.
// Globals:
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
    ScriptRuntime( LuaState& lua, AssetRegistry& assets, vector<string> callbacks );
    ~ScriptRuntime();
    ScriptRuntime( const ScriptRuntime& ) = delete;
    ScriptRuntime& operator=( const ScriptRuntime& ) = delete;

    LuaState& Lua() const { return mLua; }
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
    // The props' defaults; nil for a module that is gone.
    LuaTable Props( ScriptModuleHandle module ) const;

    // `label` names the instance in errors - the entity's path.
    // `overrides`, when given, is a table laid over the defaults; a key
    // that is not a prop, or a value of another kind than the default, is
    // an error.
    expected<ScriptInstanceHandle, string> Create( ScriptModuleHandle module, string label,
                                                   const LuaTable& overrides = {} );
    // Takes it off its events and stops its coroutines. Safe from inside
    // its own callback, and on a handle that is gone already.
    void Destroy( ScriptInstanceHandle instance );
    bool Alive( ScriptInstanceHandle instance ) const;
    bool Enabled( ScriptInstanceHandle instance ) const;
    // Its self table; nil for an instance that is gone.
    LuaTable Self( ScriptInstanceHandle instance ) const;

    // Calls the callback with self and `args`; false when it failed or the
    // instance is gone. A script error is the runtime's to handle: it logs
    // it with the label and switches the instance off until its file
    // changes, so the caller has nothing more to do about it. A file without
    // that callback, or an instance switched off, does nothing and is true.
    template <typename... Args>
    bool Call( ScriptInstanceHandle instance, u32 callback, const Args&... args )
    {
        return CallWith( instance, callback, LuaRest{ { mLua.Value( args )... } } );
    }

    // Resumes the coroutines whose wait is over, instance by instance in a
    // fixed order. A paused world does not call it.
    void Tick( f32 dt );

    // Calls every subscriber of `event` with `args`, in the order they
    // subscribed.
    template <typename... Args>
    void Emit( string_view event, const Args&... args )
    {
        EmitWith( event, LuaRest{ { mLua.Value( args )... } } );
    }

private:
    struct Module
    {
        string mChunk;
        AssetHandle<ScriptAsset> mAsset;
        LuaTable mEnvironment;
        LuaTable mProps;
        // By callback index; nil where the file has none.
        vector<LuaFunction> mCallbacks;
    };

    // A coroutine of an instance and what it waits on: seconds left, a
    // function to poll, or nil for the next tick.
    struct Task
    {
        LuaThread mThread;
        LuaValue mWait;
        bool mDone = false;
    };

    struct Instance
    {
        ScriptModuleHandle mModule;
        string mLabel;
        LuaTable mSelf;
        vector<Task> mTasks;
        // The events it is on, to take itself off them when it goes.
        str_hset mEvents;
        bool mEnabled = true;
    };

    struct Library
    {
        AssetHandle<ScriptAsset> mAsset;
        // Nil until its first require, and again after it changes.
        LuaValue mResult;
        LuaTable mEnvironment;
    };

    struct Subscriber
    {
        ScriptInstanceHandle mInstance;
        LuaFunction mFunction;
    };

    // What running a file leaves.
    struct FileResult
    {
        LuaTable mEnvironment;
        // A script's.
        LuaTable mProps;
        vector<LuaFunction> mCallbacks;
        // A library's.
        LuaValue mResult;
    };

    // Swaps in the instance whose code runs, and back.
    class CurrentScope
    {
    public:
        CurrentScope( ScriptRuntime& runtime, ScriptInstanceHandle instance );
        ~CurrentScope();
        CurrentScope( const CurrentScope& ) = delete;
        CurrentScope& operator=( const CurrentScope& ) = delete;

    private:
        ScriptRuntime& mRuntime;
        ScriptInstanceHandle mPrevious;
    };

    void RegisterGlobals();
    expected<FileResult, ScriptError> RunFile( string_view path, const AssetHandle<ScriptAsset>& asset, bool library );
    expected<FileResult, ScriptError> RunTracked( string_view path, const AssetHandle<ScriptAsset>& asset,
                                                  bool library );
    expected<void, string> CheckProps( const LuaTable& props );
    expected<void, string> CopyInto( const LuaTable& self, const LuaTable& from, bool onlyMissing );
    expected<string, string> Resolve( string_view from, string_view request ) const;
    expected<LuaValue, string> Require( const string& from, const string& request );

    // A script asset changed: it runs again, and so does everything that
    // required it - libraries at their next require, scripts at once.
    void Changed( const AssetSlotBase& slot );
    expected<void, ScriptError> Rerun( ScriptModuleHandle module );

    bool CallWith( ScriptInstanceHandle instance, u32 callback, const LuaRest& args );
    void EmitWith( string_view event, const LuaRest& args );
    void Fail( ScriptInstanceHandle instance, const ScriptError& error );
    void Start( ScriptInstanceHandle instance, const LuaFunction& fn, const LuaRest& args );
    void Resume( ScriptInstanceHandle instance, const LuaThread& thread, const LuaRest& args );
    void TickInstance( ScriptInstanceHandle instance, f32 dt );
    ScriptInstanceHandle Current( const char* what ) const;

    LuaState& mLua;
    AssetRegistry& mAssets;
    AssetListenerHandle mListener;
    vector<string> mCallbacks;
    str_hmap<string> mAliases;

    SlotMap<Module, ScriptModuleTag> mModules;
    str_hmap<ScriptModuleHandle> mModulesByPath;
    SlotMap<Instance, ScriptInstanceTag> mInstances;
    str_hmap<Library> mLibraries;
    str_hmap<vector<Subscriber>> mEvents;

    // File environment -> its path, its props, whether it is a library:
    // what props and require read about the file that called them. Weak,
    // so an environment of a file run again goes with its old code.
    LuaTable mPathOf;
    LuaTable mPropsOf;
    LuaTable mLibraryOf;
    // Metatables of a file's environment while it runs (reads go to the
    // globals) and after (a new global is an error).
    LuaTable mOpenEnvironment;
    LuaTable mStrictEnvironment;

    // File -> the libraries it required: who to rerun when one changes.
    str_hmap<str_hset> mRequires;
    // The files running now, outermost first: a library met again is a
    // require cycle.
    vector<string> mRunning;
    // Whose callback or coroutine is running: what start and on act on.
    ScriptInstanceHandle mCurrent;
};
}
