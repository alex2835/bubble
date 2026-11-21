// The animation graph's operators: states added, renamed and taken out with
// everything that names them, transitions drawn and removed, nodes placed -
// each one step of the graph's document, and the graph still one the engine
// reads.
#include "bubble/animation/animation_graph.hpp"
#include "bubble/editing/operators.hpp"
#include <doctest.h>

using namespace bubble;

namespace
{
constexpr string_view cGraph = R"({
  "parameters": { "grounded": true },
  "entry": "idle",
  "states": { "idle": {}, "fall": {}, "land": {} },
  "transitions": [
    { "from": "idle", "to": "fall", "when": "!grounded" },
    { "from": "fall", "to": "land", "when": "grounded" },
    { "from": "land", "to": "return", "exit_time": 0.9 }
  ],
  "editor": { "states": { "idle": [ 0, 0 ], "fall": [ 200, 0 ] } }
}
)";

struct Fixture
{
    hmap<string, string> mFiles{ { "animations/hero.anim", string( cGraph ) } };
    Reflection mReflection;
    ComponentTypes mComponentTypes{ mReflection };
    Operators mOperators;
    AssetCatalog mAssets;
    Workspace mWorkspace{ mComponentTypes, ProjectFiles::InMemory( mFiles ), mAssets };

    Fixture()
    {
        AddBuiltinOperators( mOperators );
        REQUIRE( Run( "document.open", R"({ "path": "animations/hero.anim" })" ) );
    }

    expected<void, string> Run( string_view id, string_view args = "null" )
    {
        return mOperators.TryRun( mWorkspace, id, *Json::TryParse( args ) );
    }

    // What the operator says is wrong, past its name.
    string Why( string_view id, string_view args )
    {
        const auto runMaybe = Run( id, args );
        REQUIRE_FALSE( runMaybe );
        const string prefix = format( "{}: ", id );
        REQUIRE( runMaybe.error().starts_with( prefix ) );
        return runMaybe.error().substr( prefix.size() );
    }

    DataDocument& Data() { return *mWorkspace.TryActiveData(); }

    // What is at `path`, as compact JSON text; "-" for nothing.
    string At( string_view path )
    {
        const auto atMaybe = Data().TryAt( *FieldPath::TryParse( path ) );
        if ( not atMaybe )
            return "-";
        string text = atMaybe->Write();
        erase_if( text, []( char c ) { return c == ' ' or c == '\n'; } );
        return text;
    }

    // The states as the engine reads them, in their order.
    vector<string> States()
    {
        const auto graphMaybe = TryParseAnimationGraph( Data().GetData().Write() );
        REQUIRE_MESSAGE( graphMaybe, graphMaybe.error() );
        vector<string> names;
        for ( const AnimationState& state : graphMaybe->mStates )
            names.push_back( state.mName );
        return names;
    }
};
}

TEST_CASE( "A state added takes the first free name and its place, one step" )
{
    Fixture f;
    REQUIRE( f.Run( "graph.add_state", R"({ "x": 40, "y": 120 })" ) );
    REQUIRE( f.Run( "graph.add_state" ) );
    CHECK( f.States() == vector<string>{ "idle", "fall", "land", "state", "state_2" } );
    CHECK( f.At( "states.state" ) == "{}" );
    CHECK( f.At( "editor.states.state" ) == "[40,120]" );
    REQUIRE( f.Run( "edit.undo" ) );
    CHECK( f.At( "states.state_2" ) == "-" );
    CHECK( f.At( "editor.states.state_2" ) == "-" );

    REQUIRE( f.Run( "graph.add_state", R"({ "name": "climb" })" ) );
    CHECK( f.At( "states.climb" ) == "{}" );
    CHECK( f.Why( "graph.add_state", R"({ "name": "climb" })" ) == "name: there is a state climb already" );
    CHECK( f.Why( "graph.add_state", R"({ "name": "Climb Up" })" ) ==
           "name: 'Climb Up' is not a state's name: a name is snake_case - a lowercase letter, then lowercase "
           "letters, digits and _" );
    CHECK( f.Why( "graph.add_state", R"({ "name": "return" })" ) ==
           "name: return is where a transition goes back to, not a state's name" );
}

TEST_CASE( "A state taken out takes its transitions, its place and the entry with it, one step" )
{
    Fixture f;
    const string written = f.Data().GetData().Write();
    REQUIRE( f.Run( "graph.remove_state", R"({ "state": "idle" })" ) );
    CHECK( f.States() == vector<string>{ "fall", "land" } );
    CHECK( f.At( "entry" ) == "-" );
    CHECK( f.At( "editor.states.idle" ) == "-" );
    CHECK( f.At( "transitions" ) ==
           R"([{"from":"fall","to":"land","when":"grounded"},{"from":"land","to":"return","exit_time":0.9}])" );
    REQUIRE( f.Run( "edit.undo" ) );
    CHECK( f.Data().GetData().Write() == written );

    CHECK( f.Why( "graph.remove_state", R"({ "state": "jump" })" ) ==
           "state: no state jump (states: idle, fall, land)" );
    REQUIRE( f.Run( "graph.remove_state", R"({ "state": "fall" })" ) );
    REQUIRE( f.Run( "graph.remove_state", R"({ "state": "land" })" ) );
    CHECK( f.Why( "graph.remove_state", R"({ "state": "idle" })" ) ==
           "idle is the graph's only state: a graph has a state at least" );
}

TEST_CASE( "A state renamed is renamed where it stands, in its transitions, the entry and its place" )
{
    Fixture f;
    const string written = f.Data().GetData().Write();
    REQUIRE( f.Run( "graph.rename_state", R"({ "state": "fall", "name": "airborne" })" ) );
    CHECK( f.States() == vector<string>{ "idle", "airborne", "land" } );
    CHECK( f.At( "transitions[1].to" ) == R"("airborne")" );
    CHECK( f.At( "transitions[2].from" ) == R"("airborne")" );
    CHECK( f.At( "editor.states" ) == R"({"idle":[0,0],"airborne":[200,0]})" );
    REQUIRE( f.Run( "graph.rename_state", R"({ "state": "idle", "name": "stand" })" ) );
    CHECK( f.At( "entry" ) == R"("stand")" );
    REQUIRE( f.Run( "edit.undo" ) );
    REQUIRE( f.Run( "edit.undo" ) );
    CHECK( f.Data().GetData().Write() == written );
    CHECK( f.Why( "graph.rename_state", R"({ "state": "idle", "name": "land" })" ) ==
           "name: there is a state land already" );
}

TEST_CASE( "A transition drawn is taken when its state's clip has played; from any state none goes back" )
{
    Fixture f;
    REQUIRE( f.Run( "graph.add_transition", R"({ "from": "*", "to": "fall" })" ) );
    CHECK( f.At( "transitions[4]" ) == R"({"from":"*","to":"fall","exit_time":1})" );
    f.States();
    CHECK( f.Why( "graph.add_transition", R"({ "from": "*", "to": "return" })" ) ==
           "from any state there is no return: return goes back to where a state was entered from" );
    CHECK( f.Why( "graph.add_transition", R"({ "from": "idle", "to": "swim" })" ) ==
           "to: no state swim (states: idle, fall, land)" );

    REQUIRE( f.Run( "graph.remove_transition", R"({ "index": 1 })" ) );
    CHECK( f.At( "transitions[1].from" ) == R"("fall")" );
    CHECK( f.Why( "graph.remove_transition", R"({ "index": 9 })" ) ==
           "index: 9 is no transition's (transitions: 1 to 3)" );

    // A graph with none yet gets its list.
    for ( int at = 0; at < 3; ++at )
        REQUIRE( f.Run( "graph.remove_transition", R"({ "index": 1 })" ) );
    REQUIRE( f.Run( "data.remove", R"({ "path": "transitions" })" ) );
    REQUIRE( f.Run( "graph.add_transition", R"({ "from": "idle", "to": "land" })" ) );
    CHECK( f.At( "transitions" ) == R"([{"from":"idle","to":"land","exit_time":1}])" );
}

TEST_CASE( "A node placed is kept under the editor: a state by its name, the any state and the return apart" )
{
    Fixture f;
    REQUIRE( f.Run( "graph.place", R"({ "state": "land", "x": 400, "y": 80 })" ) );
    REQUIRE( f.Run( "graph.place", R"({ "state": "*", "x": -200, "y": 0 })" ) );
    REQUIRE( f.Run( "graph.place", R"({ "state": "return", "x": 600, "y": 0 })" ) );
    CHECK( f.At( "editor.states.land" ) == "[400,80]" );
    CHECK( f.At( "editor.any" ) == "[-200,0]" );
    CHECK( f.At( "editor.return" ) == "[600,0]" );
    f.States();
    CHECK( f.Why( "graph.place", R"({ "state": "swim", "x": 0, "y": 0 })" ) ==
           "state: no state swim (states: idle, fall, land)" );
}

TEST_CASE( "The graph's operators wait for an animation graph" )
{
    Fixture f;
    REQUIRE( f.Run( "document.new" ) );
    CHECK( f.Why( "graph.add_state", "null" ) == "no animation graph is active: open a .anim" );
}

TEST_CASE( "A parameter renamed is renamed where it stands and wherever the graph reads it, one step" )
{
    Fixture f;
    REQUIRE( f.Run( "data.set", R"({ "path": "parameters.speed", "value": 0 })" ) );
    REQUIRE( f.Run( "data.set", R"({ "path": "states.fall.speed", "value": "speed" })" ) );
    REQUIRE( f.Run( "data.set",
                    R"({ "path": "states.land.blend", "value": { "param": "speed", "points": [ [ "a", 0 ] ] } })" ) );
    REQUIRE( f.Run( "data.set", R"({ "path": "transitions[3].when", "value": [ "speed > 1", "!grounded" ] })" ) );

    REQUIRE( f.Run( "graph.rename_parameter", R"({ "parameter": "grounded", "name": "on_ground" })" ) );
    CHECK( f.At( "parameters" ) == R"({"on_ground":true,"speed":0})" );
    CHECK( f.At( "transitions[1].when" ) == R"("!on_ground")" );
    CHECK( f.At( "transitions[2].when" ) == R"("on_ground")" );
    CHECK( f.At( "transitions[3].when" ) == R"(["speed>1","!on_ground"])" );

    REQUIRE( f.Run( "graph.rename_parameter", R"({ "parameter": "speed", "name": "pace" })" ) );
    CHECK( f.At( "states.fall.speed" ) == R"("pace")" );
    CHECK( f.At( "states.land.blend.param" ) == R"("pace")" );
    CHECK( f.At( "transitions[3].when" ) == R"(["pace>1","!on_ground"])" );
    CHECK( f.States() == vector<string>{ "idle", "fall", "land" } );

    REQUIRE( f.Run( "edit.undo" ) );
    CHECK( f.At( "states.fall.speed" ) == R"("speed")" );
    CHECK( f.Why( "graph.rename_parameter", R"({ "parameter": "jump", "name": "hop" })" ) ==
           "parameter: no parameter jump (parameters: on_ground, speed)" );
    CHECK( f.Why( "graph.rename_parameter", R"({ "parameter": "speed", "name": "on_ground" })" ) ==
           "name: there is a parameter on_ground already" );
    CHECK( f.Why( "graph.rename_parameter", R"({ "parameter": "speed", "name": "Fast" })" ) ==
           "name: 'Fast' is not a parameter's name: a name is snake_case - a lowercase letter, then lowercase "
           "letters, digits and _" );
}
