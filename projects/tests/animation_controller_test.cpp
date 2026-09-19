#include "test.hpp"
#include "engine/animation/animation_controller.hpp"
#include <nlohmann/json.hpp>

// The controller asset and its evaluation: pure over parameters, no model.

namespace
{
const char* cController = R"({
  "parameters": { "speed": 0, "grounded": true, "attack": "trigger" },
  "entry": "locomotion",
  "states": {
    "locomotion": { "blend": { "param": "speed", "points": [ ["idle", 0], ["walk", 1.5], ["run", 4] ] } },
    "jump":       { "clip": "jump", "loop": false },
    "attack":     { "clip": "attack_1", "loop": false, "speed": 1.5 }
  },
  "transitions": [
    { "from": "*",          "to": "jump",       "when": "!grounded",      "duration": 0.1, "interrupt": true },
    { "from": "jump",       "to": "locomotion", "when": "grounded",       "duration": 0.2 },
    { "from": "locomotion", "to": "attack",     "when": "attack",         "duration": 0.05 },
    { "from": "attack",     "to": "return",     "exit_time": 0.9,         "duration": 0.2 }
  ]
})";

AnimationController Load()
{
    return AnimationController::FromJson( json::parse( cController ), "test.anim" );
}

bool Fails( const char* text )
{
    try
    {
        AnimationController::FromJson( json::parse( text ), "bad.anim" );
        return false;
    }
    catch ( const std::runtime_error& ) { return true; }
}

using Frame = ControllerRuntime::Frame;
const Frame cIdle{ 0.5f, 0.4f, false, false };
}

TEST( Controller_Parse )
{
    const AnimationController c = Load();
    CHECK( c.mParameters.size() == 3 );
    CHECK( c.mStates.size() == 3 );
    CHECK( c.mTransitions.size() == 4 );
    CHECK( c.mStates[c.mEntry].mName == "locomotion" );
    CHECK( c.mStates[c.mEntry].IsBlend() );
    CHECK( c.mStates[c.mEntry].mBlend.mPoints.size() == 3 );
    CHECK( c.mStates[c.FindState( "attack" )].mSpeed == 1.5f );
    CHECK( not c.mStates[c.FindState( "jump" )].mLoop );
    CHECK( c.mTransitions[0].mFrom == Transition::cAnyState );
    CHECK( c.mTransitions[3].mTo == Transition::cReturn );
    CHECK( c.mTransitions[3].mExitTime and *c.mTransitions[3].mExitTime == 0.9f );

    // Conditions.
    CHECK( Condition::Parse( "speed > 1.5" ).mOp == Condition::Op::Gt );
    CHECK( Condition::Parse( "speed > 1.5" ).mValue == 1.5f );
    CHECK( Condition::Parse( "!grounded" ).mOp == Condition::Op::IsFalse );
    CHECK( Condition::Parse( "grounded == false" ).mOp == Condition::Op::Eq );
    CHECK( Condition::Parse( "  a   <=  2 " ).ToString() == "a <= 2" );

    // What must not load.
    CHECK( Fails( R"({ "states": {} })" ) );
    CHECK( Fails( R"({ "states": { "a": { "clip": "x", "blend": { "param": "p", "points": [["x",0]] } } } })" ) );
    CHECK( Fails( R"({ "states": { "a": { "clip": "x" } }, "transitions": [ { "from": "a", "to": "b", "when": "x" } ] })" ) );
    CHECK( Fails( R"({ "states": { "a": { "clip": "x" } }, "transitions": [ { "from": "a", "to": "a", "when": "nope" } ] })" ) );
    CHECK( Fails( R"({ "states": { "a": { "clip": "x" } }, "transitions": [ { "from": "a", "to": "a" } ] })" ) );
    CHECK( Fails( R"({ "parameters": { "p": 0 }, "states": { "a": { "clip": "x" } }, "transitions": [ { "from": "a", "to": "a", "when": "p >> 1" } ] })" ) );
}

TEST( Controller_Step )
{
    const AnimationController c = Load();
    Parameters params = c.DefaultParameters();
    ControllerRuntime runtime;

    // First step enters the entry state, with no transition time.
    auto change = runtime.Step( c, params, cIdle );
    CHECK( change and change->mState == c.FindState( "locomotion" ) and change->mDuration == 0.0f );
    CHECK( not runtime.Step( c, params, cIdle ) );

    // Trigger: fires once, and is consumed by the transition that took it.
    params.Trigger( "attack" );
    change = runtime.Step( c, params, cIdle );
    CHECK( change and change->mState == c.FindState( "attack" ) and change->mDuration == 0.05f );
    CHECK( params.Get( "attack" ) == 0.0f );
    CHECK( not runtime.Step( c, params, cIdle ) );

    // Return fires when the exit time is crossed, and goes back to where the
    // attack was entered from.
    CHECK( not runtime.Step( c, params, Frame{ 0.85f, 0.80f, false, false } ) );
    change = runtime.Step( c, params, Frame{ 0.95f, 0.85f, false, false } );
    CHECK( change and change->mState == c.FindState( "locomotion" ) and change->mDuration == 0.2f );

    // Any-state, and it may interrupt an easing pose.
    params.Set( "grounded", false );
    change = runtime.Step( c, params, Frame{ 0.1f, 0.05f, false, true } );
    CHECK( change and change->mState == c.FindState( "jump" ) );
    // ...but not back into the state already playing.
    CHECK( not runtime.Step( c, params, cIdle ) );
    // jump -> locomotion waits for the transition to settle (interrupt off).
    params.Set( "grounded", true );
    CHECK( not runtime.Step( c, params, Frame{ 0.5f, 0.4f, false, true } ) );
    change = runtime.Step( c, params, cIdle );
    CHECK( change and change->mState == c.FindState( "locomotion" ) );

    // An unconsumed trigger is dropped by the owner at the end of the frame.
    params.Trigger( "attack" );
    params.ResetTriggers();
    CHECK( not runtime.Step( c, params, cIdle ) );
}

TEST( Controller_ExitTimeAroundTheLoop )
{
    AnimationController c = AnimationController::FromJson( json::parse( R"({
      "states": { "a": { "clip": "x" }, "b": { "clip": "y" } },
      "transitions": [ { "from": "a", "to": "b", "exit_time": 0.95 } ]
    })" ), "loop.anim" );
    Parameters params;
    ControllerRuntime runtime;
    runtime.Step( c, params, cIdle );

    // 0.90 -> 0.02 wrapped past 0.95 this frame.
    auto change = runtime.Step( c, params, Frame{ 0.02f, 0.90f, true, false } );
    CHECK( change and change->mState == c.FindState( "b" ) );

    // exit_time 0 fires on the state's first frame.
    c.mTransitions[0].mExitTime = 0.0f;
    runtime = {};
    runtime.Step( c, params, cIdle );
    change = runtime.Step( c, params, Frame{ 0.0f, 0.0f, false, false } );
    CHECK( change and change->mState == c.FindState( "b" ) );
}
