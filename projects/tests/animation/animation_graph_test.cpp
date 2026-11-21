// Animation graphs as .anim files have them, and their logic step by step,
// without a model: clips' lengths are made up.
#include "bubble/animation/animation_graph.hpp"
#include <doctest.h>

using namespace bubble;

namespace
{
constexpr string_view cGraph = R"({
  "parameters": { "speed": 0, "grounded": true, "hit": "trigger" },
  "entry": "locomotion",
  "states": {
    "locomotion": { "blend": { "param": "speed", "points": [ ["run", 4], ["idle", 0], ["walk", 1.5] ] } },
    "airborne": { "clip": "fall", "root_motion": true },
    "hit": { "clip": "hit", "loop": false },
    "pose": {}
  },
  "transitions": [
    { "from": "locomotion", "to": "hit", "when": "hit", "duration": 0.05 },
    { "from": "*", "to": "airborne", "when": "!grounded", "duration": 0.15 },
    { "from": "airborne", "to": "locomotion", "when": [ "grounded", "speed >= 0" ] },
    { "from": "hit", "to": "return", "exit_time": 0.9, "duration": 0.25 }
  ],
  "events": { "walk": [ [0.75, "step"], [0.25, "step"] ], "idle": [ [0.5, "breathe"] ] },
  "editor": { "positions": { "hit": [ 1, 2 ] } }
})";

AnimationGraph Parse( string_view text )
{
    auto graphMaybe = TryParseAnimationGraph( text );
    REQUIRE_MESSAGE( graphMaybe, graphMaybe.error() );
    return std::move( *graphMaybe );
}

// Every clip a second long, but "hit" half that.
opt<f32> Durations( string_view clip )
{
    if ( clip == "missing" )
        return nullopt;
    return clip == "hit" ? 0.5f : 1.0f;
}

struct Player
{
    AnimationGraph mGraph;
    AnimationGraphState mState = StartAnimationGraph( mGraph );
    vector<string> mEvents;

    explicit Player( string_view text ) : mGraph( Parse( text ) ) {}

    void Steps( u32 count, f32 dt = 0.1f, f32 speed = 1.0f )
    {
        for ( u32 step = 0; step < count; ++step )
            StepAnimationGraph( mGraph, mState, dt, speed, &Durations, mEvents );
    }

    string State() const { return mGraph.mStates[static_cast<size_t>( mState.mState )].mName; }
    void Set( string_view name, f32 value ) { mState.mParameters[*mGraph.TryParameter( name )] = value; }
};
}

TEST_CASE( "An animation graph reads its parameters, states, transitions and events" )
{
    const AnimationGraph graph = Parse( cGraph );
    REQUIRE( graph.mParameters.size() == 3 );
    CHECK( graph.mParameters[1].mName == "grounded" );
    CHECK( graph.mParameters[1].mKind == AnimationParameterKind::Bool );
    CHECK( graph.mParameters[1].mDefault == 1.0f );
    CHECK( graph.mParameters[2].mKind == AnimationParameterKind::Trigger );
    CHECK( graph.mEntry == *graph.TryState( "locomotion" ) );
    // A blend's points in order of their values.
    const AnimationState& locomotion = graph.mStates[0];
    REQUIRE( locomotion.mBlend.size() == 3 );
    CHECK( locomotion.mBlend[0].mClip == "idle" );
    CHECK( locomotion.mBlend[2].mClip == "run" );
    CHECK_FALSE( graph.mStates[2].mLoop );
    CHECK( graph.mStates[1].mRootMotion );
    CHECK_FALSE( graph.mStates[0].mRootMotion );
    CHECK( graph.Clips() == vector<string>{ "idle", "walk", "run", "fall", "hit" } );
    REQUIRE( graph.mTransitions.size() == 4 );
    CHECK_FALSE( graph.mTransitions[1].mFrom );
    CHECK_FALSE( graph.mTransitions[3].mTo );
    CHECK( graph.mTransitions[2].mConditions.size() == 2 );
    CHECK( graph.mTransitions[2].mDuration == 0.2f );
    // Events in the order of their times.
    CHECK( graph.mEvents.at( "walk" ).front().mTime == 0.25f );
}

TEST_CASE( "An animation graph that is wrong says where and what" )
{
    const auto error = []( string_view text ) {
        const auto graphMaybe = TryParseAnimationGraph( text );
        REQUIRE_FALSE( graphMaybe );
        return graphMaybe.error();
    };
    CHECK( error( "[]" ) == "1:1: an animation graph is a JSON object, not an array of 0" );
    CHECK( error( R"({ "layers": [] })" ) ==
           "1:13: layers: not a field (fields: parameters, entry, states, transitions, events, editor)" );
    CHECK( error( R"({ "parameters": {} })" ) == "1:1: an animation graph has states" );
    CHECK( error( R"({ "states": { "a": { "clip": "x", "blend": { "param": "p", "points": [ ["y", 0] ] } } } })" )
               .ends_with( "states.a: a clip or a blend, not both" ) );
    CHECK( error( R"({ "states": { "a": { "loop": 1 } } })" ) == "1:30: states.a.loop is true or false, not 1" );
    CHECK( error( R"({ "states": { "a": { "speed": "fast" } } })" ) ==
           "1:31: states.a.speed: no parameter fast (parameters: none)" );
    CHECK( error( R"({ "states": { "a": {} }, "entry": "b" })" ) == "1:35: entry: no state b (states: a)" );
    CHECK( error( R"({ "states": { "a": {} }, "transitions": [ { "from": "a", "to": "c", "when": "x" } ] })" ) ==
           "1:64: transitions[0].to: no state c (states: a)" );
    CHECK( error( R"({ "states": { "a": {} }, "transitions": [ { "from": "*", "to": "return", "exit_time": 1 } ] })" )
               .ends_with( "transitions[0]: from \"*\" there is no \"return\"" ) );
    CHECK( error( R"({ "states": { "a": {}, "b": {} }, "transitions": [ { "from": "a", "to": "b" } ] })" )
               .ends_with( "transitions[0] has no when and no exit_time: it would be taken at once" ) );
    CHECK( error( R"({ "parameters": { "s": 0 }, "states": { "a": {}, "b": {} },
                       "transitions": [ { "from": "a", "to": "b", "when": "s >> 1" } ] })" )
               .ends_with( "transitions[0].when: \"s >> 1\" is not \"name\", \"!name\" or \"name <op> value\"" ) );
    CHECK( error( R"({ "states": { "a": {}, "b": {} }, "transitions": [ { "from": "a", "to": "b", "when": "go" } ] })" )
               .ends_with( "transitions[0].when: no parameter go (parameters: none)" ) );
    CHECK( error( R"({ "states": { "a": {} }, "events": { "a": [ [ 1.5, "late" ] ] } })" )
               .ends_with( "events.a is [ [time, name], ... ] with times from 0 to 1" ) );
    CHECK( error( R"({ "parameters": { "p": "yes" }, "states": { "a": {} } })" ) ==
           "1:24: parameters.p: a number, true or false, or \"trigger\", not \"yes\"" );
    CHECK( error( R"({ "parameters": { "Speed": 0 }, "states": { "a": {} } })" ) ==
           "1:28: parameters.Speed: a name is snake_case - a lowercase letter, then lowercase letters, digits and _" );
    CHECK( error( R"({ "states": { "run fast": {} } })" )
               .ends_with( "states.run fast: a name is snake_case - a "
                           "lowercase letter, then lowercase letters, digits "
                           "and _" ) );
    CHECK( error( R"({ "states": { "return": {} } })" ) ==
           "1:25: states.return: return is where a transition goes back to, not a state" );
}

TEST_CASE( "A blend plays the two points either side of its value, the nearest end alone outside them" )
{
    const vector<AnimationBlendPoint> points{ { "idle", 0.0f }, { "walk", 2.0f }, { "run", 4.0f } };
    const auto weights = BlendWeights( points, 1.5f );
    REQUIRE( weights.size() == 2 );
    CHECK( weights[0].mPoint == 0 );
    CHECK( weights[0].mWeight == doctest::Approx( 0.25f ) );
    CHECK( weights[1].mWeight == doctest::Approx( 0.75f ) );
    CHECK( BlendWeights( points, -1.0f ).front().mPoint == 0 );
    CHECK( BlendWeights( points, 9.0f ).front().mPoint == 2 );
    CHECK( BlendWeights( points, numeric_limits<f32>::quiet_NaN() ).front().mPoint == 0 );
    CHECK( BlendWeights( {}, 1.0f ).empty() );
}

TEST_CASE( "A graph enters its entry state and loops its clip, its events heard as they are passed" )
{
    Player player( cGraph );
    // At rest, the blend is idle's alone: a second long.
    player.Steps( 4 );
    CHECK( player.State() == "locomotion" );
    CHECK( player.mState.mPhase == doctest::Approx( 0.4f ) );
    CHECK( player.mEvents.empty() );
    player.Steps( 2 );
    CHECK( player.mEvents == vector<string>{ "breathe" } );
    // Around the seam, and passed once each.
    player.Steps( 4 );
    CHECK( player.mState.mWrapped );
    CHECK( player.mState.mPhase == doctest::Approx( 0.0f ) );
    CHECK( player.mEvents == vector<string>{ "breathe" } );
    // Walking, its events are walk's: the clip with the most weight.
    player.mEvents.clear();
    player.Set( "speed", 1.4f );
    player.Steps( 10 );
    CHECK( player.mEvents == vector<string>{ "step", "step" } );
    // The animator's speed, and a speed of 0, which stands.
    const f32 before = player.mState.mPhase;
    player.Steps( 3, 0.1f, 0.0f );
    CHECK( player.mState.mPhase == before );
}

TEST_CASE( "A trigger fires its transition once and goes; one nothing takes goes at the end of the step" )
{
    Player player( cGraph );
    player.Steps( 1 );
    player.Set( "hit", 1 );
    player.Steps( 1 );
    CHECK( player.State() == "hit" );
    CHECK( player.mState.mParameters[2] == 0 );
    CHECK( player.mState.mEnteredEasing == 0.05f );
    // A one-shot runs to its exit time and returns where it came from.
    player.Steps( 4 );
    CHECK( player.State() == "hit" );
    player.Steps( 1 );
    CHECK( player.State() == "locomotion" );

    // In the air, a trigger nothing takes is gone.
    player.Set( "grounded", 0 );
    player.Steps( 3 );
    CHECK( player.State() == "airborne" );
    player.Set( "hit", 1 );
    player.Steps( 1 );
    CHECK( player.mState.mParameters[2] == 0 );
    player.Set( "grounded", 1 );
    player.Steps( 2 );
    CHECK( player.State() == "locomotion" );
}

TEST_CASE( "While a transition eases only one that interrupts is taken; from any state never into itself" )
{
    Player player( R"({
      "parameters": { "a": false, "b": false },
      "states": { "idle": {}, "one": {}, "two": {} },
      "transitions": [
        { "from": "*", "to": "one", "when": "a", "duration": 0.5 },
        { "from": "one", "to": "two", "when": "b", "duration": 0.5 },
        { "from": "two", "to": "idle", "when": "!b", "duration": 0.5, "interrupt": true } ] })" );
    player.Steps( 1 );
    player.Set( "a", 1 );
    player.Steps( 1 );
    CHECK( player.State() == "one" );
    const u32 entered = player.mState.mEntered;
    // "a" holds still, but not into the state playing.
    player.Steps( 1 );
    CHECK( player.mState.mEntered == entered );
    // Easing for 0.5 s: "b" waits for it.
    player.Set( "a", 0 );
    player.Set( "b", 1 );
    player.Steps( 3 );
    CHECK( player.State() == "one" );
    player.Steps( 3 );
    CHECK( player.State() == "two" );
    // An interrupting one goes at once.
    player.Set( "b", 0 );
    player.Steps( 1 );
    CHECK( player.State() == "idle" );
}

TEST_CASE( "A state's length comes from its clips; one the model lacks lasts a second" )
{
    Player player( R"({ "states": { "a": { "clip": "missing", "speed": 2 } } })" );
    player.Steps( 2 );
    CHECK( player.mState.mPhase == doctest::Approx( 0.4f ) );
    CHECK( string( LeadingClip( player.mGraph, player.mState ) ) == "missing" );
}
