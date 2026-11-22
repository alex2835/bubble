#include "bubble/scripts/script_module.hpp"
#include "bubble/core/asset_path.hpp"
#include "bubble/core/log.hpp"
#include "bubble/core/profile.hpp"
#include "bubble/scene/hierarchy.hpp"
#include "bubble/scripts/script_values.hpp"
#include "bubble/types/algorithm.hpp"
#include "bubble/world/world.hpp"
#include "save_api.hpp"
#include "world_api.hpp"

namespace bubble
{
namespace
{
constexpr u32 Index( ScriptCallback callback )
{
    return static_cast<u32>( callback );
}

// Copies of self tables, for snapshots. The module owns the store and
// empties it when it goes - while its Luau state is still there - so a
// snapshot that outlives the world holds nothing of a state that is gone.
struct SelfStore
{
    hmap<u64, vector<pair<Entity, LuaTable>>> mSaved;
    u64 mNext = 0;
};

class ScriptsSnapshot final : public ModuleSnapshot
{
public:
    ScriptsSnapshot( Ref<SelfStore> store, u64 key ) : mStore( std::move( store ) ), mKey( key ) {}
    ~ScriptsSnapshot() override { mStore->mSaved.erase( mKey ); }
    ScriptsSnapshot( const ScriptsSnapshot& ) = delete;
    ScriptsSnapshot& operator=( const ScriptsSnapshot& ) = delete;

    const Ref<SelfStore>& Store() const { return mStore; }
    u64 Key() const { return mKey; }

private:
    Ref<SelfStore> mStore;
    u64 mKey = 0;
};

class Scripts final : public Module
{
public:
    Scripts( World& world, AssetRegistry& assets, const ScriptModuleOptions& options )
        : mWorld( world ),
          mAssets( assets ),
          mApi( CollectScriptApi( world.GetApp() ) ),
          mRuntime( mLua, assets, CallbackNames( mApi ) )
    {
        for ( u32 index = 0; index < mRuntime.Callbacks().size(); ++index )
            mCallbackIndex[Name::Declare( mRuntime.Callbacks()[index] )] = index;
        RegisterWorldApi( mLua, world, assets, mApi );
        RegisterSaves( mLua, world, options.mSaves, options.mGameVersion );
        mProgress = StartProgress( options.mProgress );
        mLua.Globals()["progress"] = mProgress;
        if ( options.mGlobals )
            options.mGlobals( mLua, world );
        mLua.Seal();
        // The one global table scripts write; they are compiled knowing.
        mLua.Thaw( mProgress );
        mLua.SetTimeLimit( std::chrono::milliseconds( static_cast<i64>( options.mTimeLimit * 1000.0 ) ) );
        for ( const auto& [alias, directory] : options.mAliases )
            if ( auto setMaybe = mRuntime.TrySetAlias( alias, directory ); not setMaybe )
                LogError( ".luaurc: {}", setMaybe.error() );

        world.OnAdded<Script>( [this]( Entity entity ) { Build( entity ); } );
        world.OnChanged<Script>( "asset"_n, [this]( Entity entity ) { Build( entity ); } );
        world.OnChanged<Script>( "props"_n, [this]( Entity entity ) { SetProps( entity ); } );
        world.OnRemoved<Script>( [this]( Entity entity ) {
            Stop( entity, true );
            mWorld.GetScene().Remove<ScriptInstance>( entity );
        } );
        world.AddStep( Phase::FixedUpdate, [this]( f32 dt ) {
            StartWaiting();
            CallEach( ScriptCallback::FixedUpdate, dt );
            mRuntime.Tick( dt );
        } );
        world.AddStep( Phase::Update, [this]( f32 dt ) {
            StartWaiting();
            CallEach( ScriptCallback::Update, dt );
        } );
        world.GetServices().Add( mRuntime );
        mListenerHandle = assets.OnChanged( [this]( const AssetEntryBase& entry ) { Changed( entry ); } );
        mCallsHandle = world.GetEvents().Subscribe<ScriptCall>( [this]( const ScriptCall& call ) { Heard( call ); } );
    }

    ~Scripts() override
    {
        mWorld.GetEvents().Unsubscribe( mCallsHandle );
        mAssets.RemoveListener( mListenerHandle );
        mStore->mSaved.clear();
        if ( mProgressKept )
        {
            if ( auto bytesMaybe = mLua.TryEncode( mProgress ) )
                mProgressKept->Keep( std::move( *bytesMaybe ) );
            else
                LogError( "progress is not kept for the next level: {}", bytesMaybe.error() );
        }
    }

    Ref<const ModuleSnapshot> TrySnapshot() override
    {
        vector<pair<Entity, LuaTable>> saved;
        for ( const auto& [entity, instanceHandle] : mInstances )
        {
            auto copyMaybe = mLua.TryDeepCopy( mRuntime.Self( instanceHandle ) );
            if ( not copyMaybe )
            {
                LogError( "{}: self is not kept by the snapshot: {}", Describe( mWorld.GetScene(), entity ),
                          copyMaybe.error() );
                continue;
            }
            saved.emplace_back( entity, LuaTable( *copyMaybe ) );
        }
        if ( saved.empty() )
            return nullptr;
        const u64 key = mStore->mNext++;
        mStore->mSaved.emplace( key, std::move( saved ) );
        return CreateRef<ScriptsSnapshot>( mStore, key );
    }

    void Restoring( OptRef<const ModuleSnapshot> snapshotMaybe ) override
    {
        // Nothing in the game ended: the instances go without on_destroy.
        for ( const auto& [entity, instanceHandle] : mInstances )
            mRuntime.Destroy( instanceHandle );
        mInstances.clear();
        mRestored.clear();
        if ( not snapshotMaybe )
            return;
        // Made by this module's type, in this world or another of the App;
        // another world's copies live in another state, and its instances
        // start here as new.
        const auto& snapshot = static_cast<const ScriptsSnapshot&>( *snapshotMaybe );
        if ( snapshot.Store() != mStore )
            return;
        for ( const auto& [entity, self] : mStore->mSaved.at( snapshot.Key() ) )
            mRestored.emplace( entity, self );
    }

private:
    // The engine's callbacks, then the modules'.
    static vector<string> CallbackNames( const ScriptApi& api )
    {
        vector<string> names( cScriptCallbacks.begin(), cScriptCallbacks.end() );
        for ( const ScriptApi::Callback& callback : api.Callbacks() )
            names.push_back( callback.mName );
        return names;
    }

    // A module calls a script: its instance on the target, if there is one.
    void Heard( const ScriptCall& call )
    {
        const auto found = mInstances.find( call.mTarget );
        const auto index = mCallbackIndex.find( call.mCallback );
        if ( found == mInstances.end() or index == mCallbackIndex.end() )
            return;
        LuaRest args;
        for ( const ScriptArg& arg : call.mArgs )
            args.mValues.push_back( visit( [&]( const auto& value ) { return mLua.Value( value ); }, arg ) );
        mRuntime.CallWith( found->second, index->second, args );
    }

    // What the last world left, or a new table: a new game, or bytes that
    // no longer read - logged.
    LuaTable StartProgress( const Ref<ScriptProgress>& kept )
    {
        mProgressKept = kept;
        if ( kept and not kept->Bytes().empty() )
        {
            auto decodedMaybe = mLua.TryDecode( kept->Bytes() );
            if ( decodedMaybe and decodedMaybe->Is( LuaKind::Table ) )
                return LuaTable( *decodedMaybe );
            LogError( "progress from the last level does not read: {}",
                      decodedMaybe ? "it is not a table"s : decodedMaybe.error() );
        }
        return mLua.NewTable();
    }

    // The entity's Script is new, or its file changed: the instance there
    // was goes, and a new one waits for the file.
    void Build( Entity entity )
    {
        Stop( entity, true );
        Scene& scene = mWorld.GetScene();
        const AssetId assetId = scene.Get<Script>( entity ).mAsset;
        scene.Add<ScriptInstance>( entity, ScriptInstance{ .mAssetRef = assetId ? mAssets.Load<ScriptAsset>( assetId )
                                                                                : AssetRef<ScriptAsset>(),
                                                           .mLibraryRefs = {},
                                                           .mPrefabRefs = {},
                                                           .mInstanceHandle = {},
                                                           .mFailed = false } );
    }

    void Stop( Entity entity, bool tell )
    {
        const auto found = mInstances.find( entity );
        if ( found == mInstances.end() )
            return;
        const ScriptInstanceHandle instanceHandle = found->second;
        mInstances.erase( found );
        if ( const auto runtimeMaybe = mWorld.GetScene().TryGet<ScriptInstance>( entity ) )
            runtimeMaybe->mInstanceHandle = {};
        if ( tell )
            mRuntime.Call( instanceHandle, Index( ScriptCallback::Destroy ) );
        mRuntime.Destroy( instanceHandle );
    }

    void StartWaiting()
    {
        Scene& scene = mWorld.GetScene();
        for ( const Entity entity : scene.EntitiesWith<ScriptInstance>() )
        {
            const auto runtimeMaybe = scene.TryGet<ScriptInstance>( entity );
            if ( not runtimeMaybe or runtimeMaybe->mInstanceHandle or runtimeMaybe->mFailed or
                 not runtimeMaybe->mAssetRef )
                continue;
            switch ( Holding( entity, *runtimeMaybe ) )
            {
                case AssetState::Loading: break;
                case AssetState::Failed: break;
                case AssetState::Ready: Start( entity ); break;
            }
        }
    }

    // The file and its libraries, all the way down: Ready when every one
    // is, Failed - and the entity failed, logged - when one is.
    AssetState Holding( Entity entity, ScriptInstance& runtime )
    {
        vector<AssetRef<ScriptAsset>> held{ runtime.mAssetRef };
        held.insert( held.end(), runtime.mLibraryRefs.begin(), runtime.mLibraryRefs.end() );
        bool loading = false;
        for ( size_t at = 0; at < held.size(); ++at )
        {
            const AssetRef<ScriptAsset> assetRef = held[at];
            if ( assetRef.State() == AssetState::Loading )
            {
                loading = true;
                continue;
            }
            if ( assetRef.State() == AssetState::Failed )
            {
                runtime.mFailed = true;
                LogError( "{}: {}: {}", Describe( mWorld.GetScene(), entity ), assetRef.Entry().Path().String(),
                          assetRef.Entry().Error() );
                return AssetState::Failed;
            }
            for ( const string& path : assetRef.TryGet()->mRequires )
            {
                const auto known = ranges::find_if(
                    held, [&]( const AssetRef<ScriptAsset>& other ) { return other.Entry().Path().View() == path; } );
                if ( known != held.end() )
                    continue;
                const auto validMaybe = AssetPath::TryFrom( path );
                if ( not validMaybe )
                    continue;
                AssetRef<ScriptAsset> library = mAssets.Load<ScriptAsset>( *validMaybe );
                runtime.mLibraryRefs.push_back( library );
                held.push_back( std::move( library ) );
            }
        }
        return loading ? AssetState::Loading : AssetState::Ready;
    }

    void Start( Entity entity )
    {
        Scene& scene = mWorld.GetScene();
        const string label = Describe( scene, entity );
        const string path = scene.Get<ScriptInstance>( entity ).mAssetRef.Entry().Path().String();
        // Running a file runs its top, which may change the scene: nothing
        // found before it is kept after.
        auto scriptMaybe = mRuntime.TryLoad( path );
        if ( not scriptMaybe )
        {
            Fail( entity, scriptMaybe.error().mMessage );
            return;
        }
        if ( not scene.Has<ScriptInstance>( entity ) )
            return;
        auto overridesMaybe = TryOverrides( entity );
        if ( not overridesMaybe )
        {
            Fail( entity, overridesMaybe.error() );
            return;
        }
        auto instanceMaybe = mRuntime.TryCreate( *scriptMaybe, label, *overridesMaybe );
        if ( not instanceMaybe )
        {
            // The label leads the runtime's message already.
            LogError( "{}", instanceMaybe.error() );
            scene.Get<ScriptInstance>( entity ).mFailed = true;
            return;
        }
        const ScriptInstanceHandle instanceHandle = *instanceMaybe;
        const LuaTable self = mRuntime.Self( instanceHandle );
        if ( const auto restored = mRestored.find( entity ); restored != mRestored.end() )
        {
            Replace( self, restored->second );
            mRestored.erase( restored );
        }
        self.RawSet( "entity", entity );
        HoldPrefabs( scene.Get<ScriptInstance>( entity ), self );
        scene.Get<ScriptInstance>( entity ).mInstanceHandle = instanceHandle;
        mInstances[entity] = instanceHandle;
        mRuntime.Call( instanceHandle, Index( ScriptCallback::Start ) );
    }

    // The prefabs among self's props - one, or a list of them - loading now
    // so they are in when the script spawns them.
    void HoldPrefabs( ScriptInstance& runtime, const LuaTable& self )
    {
        const auto hold = [&]( const LuaValue& value ) {
            const auto idMaybe = value.TryAs<AssetId>();
            const auto pathMaybe = idMaybe and *idMaybe ? mAssets.Catalog().TryPathOf( *idMaybe ) : nullopt;
            if ( pathMaybe and pathMaybe->View().ends_with( ".prefab" ) )
                runtime.mPrefabRefs.push_back( mAssets.Load<PrefabAsset>( *idMaybe ) );
        };
        for ( const auto& [key, value] : self.Pairs() )
        {
            hold( value );
            if ( value.Is( LuaKind::Table ) )
                for ( const auto& [index, element] : LuaTable( value ).Pairs() )
                    hold( element );
        }
    }

    // self as the snapshot had it, copied again: a snapshot restores as
    // often as it is asked.
    void Replace( const LuaTable& self, const LuaTable& saved )
    {
        auto copyMaybe = mLua.TryDeepCopy( saved );
        if ( not copyMaybe )
            return;
        for ( const auto& [key, value] : self.Pairs() )
            self.RawSet( key, LuaValue() );
        for ( const auto& [key, value] : LuaTable( *copyMaybe ).Pairs() )
            self.RawSet( key, value );
    }

    // The entity's overrides as a table for the runtime.
    expected<LuaTable, string> TryOverrides( Entity entity )
    {
        const Scene& scene = mWorld.GetScene();
        const Value& props = scene.Get<Script>( entity ).mProps;
        if ( not props.Is<Value::Record>() )
            return unexpected( format( "script.props: a record of props expected, got {}", props.Describe() ) );
        auto tableMaybe = TryLuaFromValue( mLua, props, scene, entity, "script.props" );
        if ( not tableMaybe )
            return unexpected( std::move( tableMaybe.error() ) );
        return LuaTable( *tableMaybe );
    }

    // The scene set a prop: laid on self while the game runs, the rest of
    // self kept. One that waited for its file tries again.
    void SetProps( Entity entity )
    {
        Scene& scene = mWorld.GetScene();
        const auto found = mInstances.find( entity );
        if ( found == mInstances.end() )
        {
            if ( const auto runtimeMaybe = scene.TryGet<ScriptInstance>( entity ) )
                runtimeMaybe->mFailed = false;
            return;
        }
        auto overridesMaybe = TryOverrides( entity );
        if ( not overridesMaybe )
        {
            LogError( "{}: {}", Describe( scene, entity ), overridesMaybe.error() );
            return;
        }
        if ( auto setMaybe = mRuntime.TrySetProps( found->second, *overridesMaybe ); not setMaybe )
            LogError( "{}", setMaybe.error() );
    }

    void Fail( Entity entity, const string& message )
    {
        Scene& scene = mWorld.GetScene();
        LogError( "{}: {}", Describe( scene, entity ), message );
        if ( const auto runtimeMaybe = scene.TryGet<ScriptInstance>( entity ) )
            runtimeMaybe->mFailed = true;
    }

    // A script changed: what failed may load now - its own file, or a
    // library it requires.
    void Changed( const AssetEntryBase& entry )
    {
        if ( entry.Type() != type_index( typeid( ScriptAsset ) ) )
            return;
        Scene& scene = mWorld.GetScene();
        for ( const Entity entity : scene.EntitiesWith<ScriptInstance>() )
            scene.Get<ScriptInstance>( entity ).mFailed = false;
    }

    void CallEach( ScriptCallback callback, f32 dt )
    {
        BUBBLE_PROFILE_ZONE_NAMED( "Scripts" );
        Scene& scene = mWorld.GetScene();
        for ( const Entity entity : scene.EntitiesWith<ScriptInstance>() )
        {
            // One a callback before it destroyed is gone already.
            const auto runtimeMaybe = scene.TryGet<ScriptInstance>( entity );
            if ( runtimeMaybe and runtimeMaybe->mInstanceHandle )
                mRuntime.Call( runtimeMaybe->mInstanceHandle, Index( callback ), dt );
        }
    }

    World& mWorld;
    AssetRegistry& mAssets;
    ScriptApi mApi;
    // The state first: everything below holds values of it.
    LuaState mLua{ PrintToLog };
    ScriptRuntime mRuntime;
    hmap<Name, u32> mCallbackIndex;
    AssetListenerHandle mListenerHandle;
    EventHandle mCallsHandle;
    // By entity, for when the entity and its ScriptInstance are gone.
    hmap<Entity, ScriptInstanceHandle> mInstances;
    Ref<SelfStore> mStore = CreateRef<SelfStore>();
    // What a Restore brought back, until the entity's instance starts.
    hmap<Entity, LuaTable> mRestored;
    LuaTable mProgress;
    Ref<ScriptProgress> mProgressKept;
};
}

ModuleType ScriptModule( AssetRegistry& assets, ScriptModuleOptions options )
{
    return ModuleType{
        .mName = "scripts",
        .mDescribe =
            []( Reflection& reflection, ComponentTypes& componentTypes ) {
                reflection.Type<Script>( "script" )
                    .Field<&Script::mAsset>( "asset", { .mAssetKind = cScriptKind.data() } )
                    .Field<&Script::mProps>( "props" );
                componentTypes.Add<Script>( "scripts" );
            },
        .mCreate = [&assets, options = std::move( options )]( World& world ) -> Scope<Module> {
            return CreateScope<Scripts>( world, assets, options );
        },
    };
}
}
