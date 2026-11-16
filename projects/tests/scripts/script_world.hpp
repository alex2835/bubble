#pragma once
// Worlds with scripts: project files in memory - the import cache too -
// the script importer compiling them, a registry reading the cache, App
// with the transform, scripts and - after them, as scripts may not know -
// render modules, and record( text ) in every world's globals for
// observing order.
#include "bubble/import/import_cache.hpp"
#include "bubble/import/script_importer.hpp"
#include "bubble/render/render_scene.hpp"
#include "bubble/scene/hierarchy.hpp"
#include "bubble/scripts/script_module.hpp"
#include "bubble/world/transform.hpp"
#include "bubble/world/world.hpp"
#include "scripts/script_helpers.hpp"

namespace bubble::test
{
inline Text MakeText( string_view utf8 )
{
    return *Text::TryFrom( utf8 );
}

struct ScriptWorld
{
    // Before App: Jobs finishes the loads still waiting when it goes, and
    // they read the files.
    hmap<string, string> mFiles;
    AssetCatalog mCatalog;
    Importer mImporter = ScriptImporter();
    App mApp{ AppOptions{ .mThreads = 1 } };
    AssetRegistry mAssets{ mCatalog, AssetRegistry::AtOnce( [this]( const AssetId& assetId, string_view kind ) {
                               const auto found = mFiles.find( ImportedPath( assetId, kind ).View() );
                               if ( found == mFiles.end() )
                                   return expected<string, string>( unexpected( "no such file"s ) );
                               return expected<string, string>( found->second );
                           } ),
                           mApp.GetJobs() };
    // What scripts passed to record( text ), in order, whichever world.
    vector<string> mRecorded;
    // Shared by every world made here, one after another as levels are.
    Ref<ScriptProgress> mProgress = CreateRef<ScriptProgress>();
    Ref<SaveSlots> mSaves = SaveSlotsInMemory();

    // Without the render module, a world has no debug_draw to draw with.
    explicit ScriptWorld( bool render = true, f64 timeLimit = 1.0 )
    {
        RegisterScriptLoader( mAssets );
        mApp.AddModule( TransformModule() );
        mApp.AddModule(
            ScriptModule( mAssets, ScriptModuleOptions{ .mGlobals =
                                                            [this]( LuaState& lua, World& ) {
                                                                lua.Globals()["record"] = [this]( string text ) {
                                                                    mRecorded.push_back( std::move( text ) );
                                                                };
                                                            },
                                                        .mAliases = {},
                                                        .mTimeLimit = timeLimit,
                                                        .mProgress = mProgress,
                                                        .mSaves = mSaves,
                                                        .mGameVersion = "test 1.0" } ) );
        if ( render )
            mApp.AddModule( RenderModule( mAssets ) );
    }

    // Writes the file and imports it, as the editor does as files appear.
    AssetId Write( string_view path, string_view source )
    {
        mFiles[string( path )] = string( source );
        const AssetPath assetPath = *AssetPath::TryFrom( path );
        if ( not mCatalog.TryIdOf( assetPath ) )
            mCatalog.Add( AssetId::Generate(), assetPath );
        const AssetId assetId = *mCatalog.TryIdOf( assetPath );
        const auto importedMaybe =
            TryImportAsset( ProjectFiles::InMemory( mFiles ), mImporter, assetPath, assetId, Json( Json::Object() ) );
        REQUIRE_MESSAGE( importedMaybe, importedMaybe.error() );
        return assetId;
    }

    // The file changed on disk: imported again, and the registry reloads it.
    void Change( string_view path, string_view source )
    {
        Write( path, source );
        const auto reloadedMaybe = mAssets.TryReload( *AssetPath::TryFrom( path ) );
        REQUIRE_MESSAGE( reloadedMaybe, reloadedMaybe.error() );
    }

    // An entity under the root with the script and its overrides.
    static Entity Add( Scene& scene, string_view name, AssetId script, Value::Record props = {} )
    {
        const Entity entity = CreateChild( scene, scene.Root(), MakeText( name ) );
        scene.Add<Script>( entity, Script{ .mAsset = script, .mProps = Value( std::move( props ) ) } );
        return entity;
    }

    // Frames with no fixed step until the loads are in: the scripts loaded
    // start, each running on_start and on_update once.
    void Load( World& world, u32 frames = 4 )
    {
        for ( u32 frame = 0; frame < frames; ++frame )
        {
            mApp.GetJobs().RunMainQueue();
            world.Frame( 0.0 );
        }
    }

    // The scene's override of a prop set while the world runs.
    void SetProps( World& world, Entity entity, Value::Record props )
    {
        const ComponentTypes& componentTypes = mApp.GetComponentTypes();
        const auto setMaybe =
            componentTypes.TrySetField( world.GetScene(), entity, *componentTypes.TryFind( "script"_n ),
                                        *FieldPath::TryParse( "props" ), MetaAny( Value( std::move( props ) ) ) );
        REQUIRE_MESSAGE( setMaybe, setMaybe.error() );
    }

    LuaTable Self( World& world, Entity entity )
    {
        return world.Get<ScriptRuntime>().Self( world.GetScene().Get<ScriptInstance>( entity ).mInstanceHandle );
    }

    vector<string> TakeRecorded() { return std::exchange( mRecorded, {} ); }
};

inline ValueField Prop( string_view name, Value value )
{
    return ValueField{ Name::Declare( name ), std::move( value ) };
}
}
