// The project's files as operators change them: an asset made from its
// template, a file renamed with its .import - the catalog and an open
// document following -, a file deleted unless it is open.
#include "bubble/editing/operators.hpp"
#include <doctest.h>

using namespace bubble;

namespace
{
struct Fixture
{
    hmap<string, string> mFiles{
        { "models/crate.glb", "glb" },
        { "models/crate.glb.import",
          R"({ "version": 1, "asset": "c4a7e000c4a7e000c4a7e000c4a7e000", "settings": {} })" }
    };
    Reflection mReflection;
    ComponentTypes mComponentTypes{ mReflection };
    Operators mOperators;
    AssetCatalog mAssets;
    Workspace mWorkspace{ mComponentTypes, ProjectFiles::InMemory( mFiles ), mAssets };
    const AssetId mCrate = *AssetId::TryParse( "c4a7e000c4a7e000c4a7e000c4a7e000" );

    Fixture()
    {
        AddBuiltinOperators( mOperators );
        mAssets.Add( mCrate, *AssetPath::TryFrom( "models/crate.glb" ) );
    }

    expected<void, string> Run( string_view id, string_view args )
    {
        return mOperators.TryRun( mWorkspace, id, *Json::TryParse( args ) );
    }
};
}

TEST_CASE( "asset.create writes a new file from its kind's template, and never over one" )
{
    Fixture f;
    for ( const string_view path : { "materials/red.mat", "prefabs/door.prefab", "levels/cave.scene",
                                     "animations/door.anim", "scripts/door.luau" } )
    {
        const auto madeMaybe = f.Run( "asset.create", format( R"({{ "path": "{}" }})", path ) );
        REQUIRE_MESSAGE( madeMaybe, madeMaybe.error() );
        CHECK( f.mFiles.contains( string( path ) ) );
    }
    CHECK( f.mFiles.at( "scripts/door.luau" ).starts_with( "local P = props {}" ) );
    CHECK( f.Run( "asset.create", R"({ "path": "materials/red.mat" })" ).error() ==
           "asset.create: materials/red.mat is there already" );
    CHECK( f.Run( "asset.create", R"({ "path": "notes.txt" })" ).error() ==
           "asset.create: notes.txt: an asset is made as .mat, .prefab, .scene, .anim or .luau" );
}

TEST_CASE( "asset.rename moves a file with its .import; the catalog and an open document follow" )
{
    Fixture f;
    REQUIRE( f.Run( "asset.rename", R"({ "path": "models/crate.glb", "to": "props/box.glb" })" ) );
    CHECK( not f.mFiles.contains( "models/crate.glb" ) );
    CHECK( f.mFiles.contains( "props/box.glb" ) );
    CHECK( f.mFiles.contains( "props/box.glb.import" ) );
    CHECK( not f.mFiles.contains( "models/crate.glb.import" ) );
    CHECK( f.mAssets.TryPathOf( f.mCrate )->View() == "props/box.glb" );

    REQUIRE( f.Run( "asset.create", R"({ "path": "cave.scene" })" ) );
    REQUIRE( f.Run( "document.open", R"({ "path": "cave.scene" })" ) );
    REQUIRE( f.Run( "asset.rename", R"({ "path": "cave.scene", "to": "levels/cave.scene" })" ) );
    CHECK( f.mWorkspace.TryActive()->TryPath()->View() == "levels/cave.scene" );

    CHECK( f.Run( "asset.rename", R"({ "path": "props/box.glb", "to": "levels/cave.scene" })" ).error() ==
           "asset.rename: levels/cave.scene is there already" );
    CHECK( f.Run( "asset.rename", R"({ "path": "gone.glb", "to": "here.glb" })" ).error() ==
           "asset.rename: gone.glb: no such file" );
}

TEST_CASE( "asset.delete takes a file and its .import, but not one open as a document" )
{
    Fixture f;
    REQUIRE( f.Run( "asset.delete", R"({ "path": "models/crate.glb" })" ) );
    CHECK( f.mFiles.empty() );
    CHECK( not f.mAssets.TryPathOf( f.mCrate ) );

    REQUIRE( f.Run( "asset.create", R"({ "path": "cave.scene" })" ) );
    REQUIRE( f.Run( "document.open", R"({ "path": "cave.scene" })" ) );
    CHECK( f.Run( "asset.delete", R"({ "path": "cave.scene" })" ).error() ==
           "asset.delete: cave.scene is open: close it first" );
}
