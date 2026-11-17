#pragma once
// Worlds with physics: App with the transform, scripts and physics modules,
// a registry over an import cache in memory - models for mesh colliders,
// scripts compiled by the importer -, record( text ) in every world's
// globals, and helpers to place bodies and step.
#include "bubble/assets/model_primitives.hpp"
#include "bubble/import/import_cache.hpp"
#include "bubble/import/script_importer.hpp"
#include "bubble/physics/physics.hpp"
#include "bubble/scene/hierarchy.hpp"
#include "bubble/scripts/script_module.hpp"
#include "bubble/world/transform.hpp"
#include "bubble/world/world.hpp"
#include "log_watch.hpp"
#include <doctest.h>

namespace bubble::test
{
struct PhysicsFixture
{
    // Before App: Jobs finishes the loads still waiting when it goes.
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
    vector<string> mRecorded;

    PhysicsFixture()
    {
        RegisterScriptLoader( mAssets );
        RegisterModelLoader( mAssets );
        mApp.AddModule( TransformModule() );
        mApp.AddModule(
            ScriptModule( mAssets, ScriptModuleOptions{ .mGlobals =
                                                            [this]( LuaState& lua, World& ) {
                                                                lua.Globals()["record"] = [this]( string text ) {
                                                                    mRecorded.push_back( std::move( text ) );
                                                                };
                                                            },
                                                        .mAliases = {},
                                                        .mTimeLimit = 1.0,
                                                        .mProgress = {},
                                                        .mSaves = {},
                                                        .mGameVersion = {} } ) );
        mApp.AddModule( PhysicsModule( mAssets ) );
    }

    AssetId Add( string_view path, string bytes, string_view kind )
    {
        const AssetPath assetPath = *AssetPath::TryFrom( path );
        if ( not mCatalog.TryIdOf( assetPath ) )
            mCatalog.Add( AssetId::Generate(), assetPath );
        const AssetId assetId = *mCatalog.TryIdOf( assetPath );
        mFiles[ImportedPath( assetId, kind ).String()] = std::move( bytes );
        return assetId;
    }

    AssetId AddModel( string_view path, const ModelAsset& model )
    {
        return Add( path, WriteModelAsset( model ), cModelKind );
    }

    AssetId AddScript( string_view path, string_view source )
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

    static Entity Place( Scene& scene, string_view name, Transform transform, Entity parent = Entity::Null )
    {
        const Entity entity =
            CreateChild( scene, parent == Entity::Null ? scene.Root() : parent, *Text::TryFrom( name ) );
        scene.Add<Transform>( entity, transform );
        return entity;
    }

    // A floor 20 across whose top is at y = 0.
    static Entity Ground( Scene& scene )
    {
        const Entity ground = Place( scene, "ground", Transform{ .mPosition = { 0.0f, -0.5f, 0.0f } } );
        scene.Add<BoxCollider>( ground, BoxCollider{ .mSize = { 20.0f, 1.0f, 20.0f } } );
        return ground;
    }

    // A unit box that falls from `position`.
    static Entity Crate( Scene& scene, string_view name, vec3 position )
    {
        const Entity crate = Place( scene, name, Transform{ .mPosition = position } );
        scene.Add<BoxCollider>( crate );
        scene.Add<RigidBody>( crate );
        return crate;
    }

    void Steps( World& world, u32 count )
    {
        for ( u32 step = 0; step < count; ++step )
        {
            mApp.GetJobs().RunMainQueue();
            world.Step();
        }
    }

    static vec3 PositionOf( World& world, Entity entity )
    {
        return world.GetScene().Get<Transform>( entity ).mPosition;
    }

    vector<string> TakeRecorded() { return std::exchange( mRecorded, {} ); }
};
}
