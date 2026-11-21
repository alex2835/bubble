// Data documents: a material or an animation graph open as its JSON, with a
// history of its own - values set and taken out by operators, a drag folded
// into one step, saved, closed, and active beside the scenes.
#include "bubble/editing/operators.hpp"
#include <doctest.h>

using namespace bubble;

namespace
{
constexpr string_view cMaterial = R"({
  "version": 1,
  "params": {
    "roughness": 0.8
  }
}
)";

struct Fixture
{
    hmap<string, string> mFiles{ { "materials/stone.mat", string( cMaterial ) } };
    Reflection mReflection;
    ComponentTypes mComponentTypes{ mReflection };
    Operators mOperators;
    AssetCatalog mAssets;
    Workspace mWorkspace{ mComponentTypes, ProjectFiles::InMemory( mFiles ), mAssets };

    Fixture()
    {
        AddBuiltinOperators( mOperators );
        REQUIRE( Run( "document.open", R"({ "path": "materials/stone.mat" })" ) );
    }

    expected<void, string> Run( string_view id, string_view args = "null" )
    {
        return mOperators.TryRun( mWorkspace, id, *Json::TryParse( args ) );
    }

    DataDocument& Data() { return *mWorkspace.TryActiveData(); }

    // What is at `path` in the open material, as JSON text; "-" for nothing.
    string At( string_view path )
    {
        const auto atMaybe = Data().TryAt( *FieldPath::TryParse( path ) );
        if ( not atMaybe )
            return "-";
        string text = atMaybe->Write();
        while ( text.ends_with( '\n' ) )
            text.pop_back();
        return text;
    }
};
}

TEST_CASE( "A material opens as a data document, active instead of any scene, its values set and undone" )
{
    Fixture f;
    CHECK( f.Data().GetPath().View() == "materials/stone.mat" );
    CHECK( not f.mWorkspace.TryActive() );
    CHECK( f.At( "params.roughness" ) == "0.8" );

    REQUIRE( f.Run( "data.set", R"({ "path": "params.roughness", "value": 0.3 })" ) );
    CHECK( f.At( "params.roughness" ) == "0.3" );
    CHECK( f.Data().IsDirty() );
    // A member that is not there is made, and the objects on its way.
    REQUIRE( f.Run( "data.set", R"({ "path": "params.tint.strength", "value": 2 })" ) );
    CHECK( f.At( "params.tint" ) == R"({ "strength": 2 })" );

    REQUIRE( f.Run( "edit.undo" ) );
    CHECK( f.At( "params.tint" ) == "-" );
    REQUIRE( f.Run( "edit.undo" ) );
    CHECK( f.At( "params.roughness" ) == "0.8" );
    CHECK( not f.Data().IsDirty() );
    REQUIRE( f.Run( "edit.redo" ) );
    CHECK( f.At( "params.roughness" ) == "0.3" );

    REQUIRE( f.Run( "data.remove", R"({ "path": "params.roughness" })" ) );
    CHECK( f.At( "params.roughness" ) == "-" );
    CHECK( f.Run( "data.remove", R"({ "path": "params.roughness" })" ).error() ==
           "data.remove: params.roughness: not there" );
    CHECK( f.Run( "data.set", R"({ "path": "version.major", "value": 2 })" ).error() ==
           "data.set: version.major: version is not an object" );
}

TEST_CASE( "A value set again and again in a group is one step, from the first value to the last" )
{
    Fixture f;
    const u64 version = f.Data().Version();
    f.Data().BeginGroup( "Drag" );
    for ( const string_view value : { "0.7", "0.6", "0.5" } )
        REQUIRE( f.Run( "data.set", format( R"({{ "path": "params.roughness", "value": {} }})", value ) ) );
    f.Data().EndGroup();
    CHECK( f.Data().Version() > version );
    CHECK( f.At( "params.roughness" ) == "0.5" );
    REQUIRE( f.Run( "edit.undo" ) );
    CHECK( f.At( "params.roughness" ) == "0.8" );
    CHECK( f.Run( "edit.undo" ).error() == "edit.undo: nothing to undo" );
}

TEST_CASE( "A data document saves to its file, keeps what is not saved unless told, and gives the scene back" )
{
    Fixture f;
    REQUIRE( f.Run( "data.set", R"({ "path": "params.roughness", "value": 0.25 })" ) );
    CHECK( f.Run( "document.close" ).error() ==
           "document.close: it has changes not saved: save it, or close it with discard" );
    REQUIRE( f.Run( "document.save" ) );
    CHECK( not f.Data().IsDirty() );
    CHECK( f.mFiles.at( "materials/stone.mat" ).find( "0.25" ) != string::npos );

    // Renamed as a file, the open document follows.
    REQUIRE( f.Run( "asset.rename", R"({ "path": "materials/stone.mat", "to": "materials/rock.mat" })" ) );
    CHECK( f.Data().GetPath().View() == "materials/rock.mat" );

    f.mWorkspace.Open();
    CHECK( not f.mWorkspace.TryActiveData() );
    REQUIRE( f.mWorkspace.TryActive() );
    REQUIRE( f.Run( "document.open", R"({ "path": "materials/rock.mat" })" ) );
    REQUIRE( f.Run( "document.close" ) );
    CHECK( f.mWorkspace.AllData().empty() );
    CHECK( f.mWorkspace.TryActive() );
}
