#pragma once
#include "bubble/assets/asset_registry.hpp"
#include "bubble/assets/prefab_asset.hpp"
#include "bubble/data/value.hpp"
#include "bubble/scripts/script_runtime.hpp"
#include "bubble/scripts/script_saves.hpp"
#include "bubble/world/module.hpp"

namespace bubble
{
class World;

// A script on an entity: its file, and the props the scene sets other than
// the file does - a record by prop name, laid over the file's defaults.
struct Script
{
    AssetId mAsset;
    Value mProps = Value( Value::Record{} );
};

// What the scripts module built from an entity's Script; never saved or
// copied.
struct ScriptInstance
{
    AssetRef<ScriptAsset> mAssetRef;
    // What the file requires, and what that requires: loaded before it
    // runs, as require itself never loads.
    vector<AssetRef<ScriptAsset>> mLibraryRefs;
    // The prefabs its props name, loading as it starts and held while it
    // runs: what spawn takes ready.
    vector<AssetRef<PrefabAsset>> mPrefabRefs;
    // Empty until the file is loaded and the instance made and started.
    ScriptInstanceHandle mInstanceHandle;
    // Its file did not load, or its props do not fit the file: logged, and
    // it waits for a script to change.
    bool mFailed = false;
};

// The callbacks a world calls, in ScriptRuntime's order.
enum class ScriptCallback : u32
{
    // on_start( self ): first, once the file is loaded - and again after a
    // world's Restore, with self as the snapshot had it.
    Start,
    // on_fixed_update( self, dt ): each fixed step; coroutines resume after.
    FixedUpdate,
    // on_update( self, dt ): each frame, a paused one too.
    Update,
    // on_destroy( self ): the script or its entity is gone - the entity
    // already, when it was destroyed. A world that goes calls none.
    Destroy,
};

// The callbacks' names, in ScriptCallback's order: what scripts define.
inline constexpr array<string_view, 4> cScriptCallbacks{ "on_start", "on_fixed_update", "on_update", "on_destroy" };

struct ScriptModuleOptions
{
    // The game's own globals - and a test's - put into each world's state
    // before it is sealed.
    function<void( LuaState&, World& )> mGlobals;
    // The project's .luaurc aliases: "lib" -> "scripts/lib".
    hmap<string, string> mAliases;
    // How long one call into a script may run - a callback, a coroutine's
    // turn, a file's top - before it fails as an endless loop, in seconds.
    f64 mTimeLimit = 1.0;
    // What the game remembers between levels, `progress` in scripts: a
    // world starts from it and leaves its own when it goes - entities in it
    // empty, as the next level is another. None: each world its own.
    Ref<ScriptProgress> mProgress;
    // Where save.write and save.read go; none, and they are errors.
    Ref<SaveSlots> mSaves;
    // What a slot's header says wrote it.
    string mGameVersion;
};

// A world's scripts: one Luau state and ScriptRuntime a world (a service),
// the engine's types in it - entity, color, asset - and an instance for
// each entity's Script, made at the first fixed step or frame its file and
// the libraries it requires, all the way down, are loaded; self.entity its
// entity. Instances go through a phase in the
// order their entities' ScriptInstances lie, which a snapshot keeps. A
// change of a prop in the scene lays the props on self anew. A world's
// snapshot keeps each self; restored into the same world, instances start
// again from it - coroutines and subscriptions anew - and into another
// world, from the scene, as new.
ModuleType ScriptModule( AssetRegistry& assets, ScriptModuleOptions options = {} );
}
