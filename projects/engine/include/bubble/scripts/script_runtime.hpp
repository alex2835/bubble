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
using ScriptHandle = Handle<struct ScriptTag>;
// One entity's run of a script: its self table - the props with the
// entity's overrides, the locals, and whatever the script keeps there - with
// its coroutines and subscriptions, which go when it goes.
using ScriptInstanceHandle = Handle<struct ScriptInstanceTag>;

// The scripts of one World: the functions every script file sees, the
// files run and the instances alive. It owns them all; the rest of the
// engine holds handles, and a handle whose instance is gone finds nothing
// rather than something else. Built on a LuaState before it is sealed.
// Globals:
//   props { name = default, ... }   the file's properties, declared once
//   locals { name = start, ... }    each instance's own state: in self like
//                                   props, but not set by the scene or saved
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
    // its callbacks. The same path again gives the same script. The asset
    // must be loaded in the registry and the state sealed.
    expected<ScriptHandle, ScriptError> Load( string_view path );
    // Destroys the script's instances with it.
    void Unload( ScriptHandle scriptHandle );
    bool Has( ScriptHandle scriptHandle, u32 callback ) const;
    // The props' defaults; nil for a script that is gone.
    LuaTable Props( ScriptHandle scriptHandle ) const;

    // `label` names the instance in errors - the entity's path.
    // `overrides`, when given, is a table laid over the defaults; a key
    // that is not a prop (a local included), or a value of another kind
    // than the default, is an error.
    expected<ScriptInstanceHandle, string> Create( ScriptHandle scriptHandle,
                                                   string label,
                                                   const LuaTable& overrides = {} );
    // Takes it off its events and stops its coroutines. Safe from inside
    // its own callback, and on a handle that is gone already.
    void Destroy( ScriptInstanceHandle instanceHandle );
    bool Alive( ScriptInstanceHandle instanceHandle ) const;
    bool Enabled( ScriptInstanceHandle instanceHandle ) const;
    // Its self table; nil for an instance that is gone.
    LuaTable Self( ScriptInstanceHandle instanceHandle ) const;

    // Calls the callback with self and `args`; false when it failed or the
    // instance is gone. A script error is the runtime's to handle: it logs
    // it with the label and switches the instance off until its file
    // changes, so the caller has nothing more to do about it. A file without
    // that callback, or an instance switched off, does nothing and is true.
    template <typename... Args>
    bool Call( ScriptInstanceHandle instanceHandle, u32 callback, const Args&... args )
    {
        return CallWith( instanceHandle, callback, LuaRest{ { mLua.Value( args )... } } );
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
    struct Script
    {
        string mPath;
        AssetRef<ScriptAsset> mAssetRef;
        LuaTable mEnvironment;
        LuaTable mProps;
        // State each instance starts with, not set from the scene.
        LuaTable mLocals;
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
        ScriptHandle mScriptHandle;
        string mLabel;
        LuaTable mSelf;
        vector<Task> mTasks;
        // The events it is on, to take itself off them when it goes.
        hset<string> mEvents;
        bool mEnabled = true;
    };

    struct Library
    {
        AssetRef<ScriptAsset> mAssetRef;
        // Nil until its first require, and again after it changes.
        LuaValue mResult;
        LuaTable mEnvironment;
    };

    struct Subscriber
    {
        ScriptInstanceHandle mInstanceHandle;
        LuaFunction mFunction;
    };

    // What running an entity script leaves.
    struct ScriptFile
    {
        LuaTable mEnvironment;
        LuaTable mProps;
        LuaTable mLocals;
        vector<LuaFunction> mCallbacks;
    };

    // What running a library leaves.
    struct LibraryFile
    {
        LuaTable mEnvironment;
        LuaValue mResult;
    };

    // Swaps in the instance whose code runs, and back.
    class CurrentScope
    {
    public:
        CurrentScope( ScriptRuntime& runtime, ScriptInstanceHandle instanceHandle );
        ~CurrentScope();
        CurrentScope( const CurrentScope& ) = delete;
        CurrentScope& operator=( const CurrentScope& ) = delete;

    private:
        ScriptRuntime& mRuntime;
        ScriptInstanceHandle mPreviousHandle;
    };

    void RegisterGlobals();
    LuaTable NewFileEnvironment( string_view path );
    // Runs the top of a file in `env`, once; after it, a new global is an
    // error. Returns what the file returned.
    expected<LuaValue, ScriptError> RunFile( string_view path,
                                             const AssetRef<ScriptAsset>& assetRef,
                                             const LuaTable& env );
    // What `passport` keeps for the file run in `env`; an empty table, kept,
    // when it keeps nothing yet.
    LuaTable PassportOf( LuaTable& passport, const LuaTable& env );
    expected<ScriptFile, ScriptError> RunScript( string_view path, const AssetRef<ScriptAsset>& assetRef );
    expected<LibraryFile, ScriptError> RunLibrary( string_view path, const AssetRef<ScriptAsset>& assetRef );
    expected<void, string> CheckProps( const LuaTable& props );
    expected<void, string> CheckLocals( const LuaTable& locals, const LuaTable& props );
    expected<void, string> CopyInto( const LuaTable& self, const LuaTable& from, bool onlyMissing );
    expected<string, string> Resolve( string_view from, string_view request ) const;
    // `file` is the environment of the code that called require.
    expected<LuaValue, string> Require( const LuaTable& file, const string& request );
    // Whether the code run in `env` required any of `libraries`.
    bool Requires( const LuaTable& env, const hset<string>& libraries );

    // A script asset changed: it runs again, and so does everything that
    // required it - libraries at their next require, scripts at once.
    void Changed( const AssetEntryBase& entry );
    expected<void, ScriptError> Rerun( ScriptHandle scriptHandle );

    bool CallWith( ScriptInstanceHandle instanceHandle, u32 callback, const LuaRest& args );
    void EmitWith( string_view event, const LuaRest& args );
    void Fail( ScriptInstanceHandle instanceHandle, const ScriptError& error );
    void Start( ScriptInstanceHandle instanceHandle, const LuaFunction& fn, const LuaRest& args );
    void Resume( ScriptInstanceHandle instanceHandle, const LuaThread& thread, const LuaRest& args );
    void TickInstance( ScriptInstanceHandle instanceHandle, f32 dt );
    ScriptInstanceHandle Current( const char* what ) const;

    LuaState& mLua;
    AssetRegistry& mAssets;
    AssetListenerHandle mListenerHandle;
    vector<string> mCallbacks;
    hmap<string, string> mAliases;

    SlotMap<Script, ScriptTag> mScripts;
    hmap<string, ScriptHandle> mScriptsByPath;
    SlotMap<Instance, ScriptInstanceTag> mInstances;
    hmap<string, Library> mLibraries;
    hmap<string, vector<Subscriber>> mEvents;

    // A file's passport, by the environment of one run of it: its path,
    // its props and locals, whether it is a library, and the libraries its
    // code required - who to run again when one changes. Weak: a run that
    // failed, or one a newer run replaced, goes with all it recorded.
    LuaTable mPathOf;
    LuaTable mPropsOf;
    LuaTable mLocalsOf;
    LuaTable mLibraryOf;
    LuaTable mRequiresOf;
    // Metatables of a file's environment while it runs (reads go to the
    // globals) and after (a new global is an error).
    LuaTable mOpenEnvironment;
    LuaTable mStrictEnvironment;

    // The files running now, outermost first: a library met again is a
    // require cycle.
    vector<string> mRunning;
    // Whose callback or coroutine is running: what start and on act on.
    ScriptInstanceHandle mCurrentHandle;
};
}
