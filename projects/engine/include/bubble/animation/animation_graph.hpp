#pragma once
#include "bubble/types/containers.hpp"
#include "bubble/types/function.hpp"
#include "bubble/types/number.hpp"
#include "bubble/types/string.hpp"
#include "bubble/types/utility.hpp"

namespace bubble
{
// An animation graph, a .anim file: the states a character can be in, what
// each one plays, and when it goes from one to another. A script never
// picks a clip: it sets parameters - speed = 3.2, trigger( "hit" ) - and the
// graph decides what that means for this character, so characters with the
// same clip names share one graph. Clips are named as the animator's model
// has them.
//
//   {
//     "parameters": { "speed": 0, "grounded": true, "hit": "trigger" },
//     "entry": "locomotion",
//     "states": {
//       "locomotion": { "blend": { "param": "speed",
//                                  "points": [ ["idle", 0], ["walk", 1.5], ["run", 4] ] } },
//       "airborne":   { "clip": "fall" },
//       "hit":        { "clip": "hit", "loop": false, "speed": "hit_speed" }
//     },
//     "transitions": [
//       { "from": "*",          "to": "airborne",   "when": "!grounded", "duration": 0.15 },
//       { "from": "airborne",   "to": "locomotion", "when": "grounded",  "duration": 0.15 },
//       { "from": "locomotion", "to": "hit",        "when": "hit",       "duration": 0.05 },
//       { "from": "hit",        "to": "return",     "exit_time": 0.9,    "duration": 0.25 }
//     ],
//     "events": { "walk": [ [0.25, "step"], [0.75, "step"] ] }
//   }
//
// States and parameters are named in snake_case, as a script names them;
// no state is called "return".
//
// A parameter is a number, true or false, or "trigger": a bool a script
// sets and the graph clears - the transition it fires takes it, and one
// nothing took is gone at the end of the step: a queued attack does not go
// off two seconds later.
//
// A state plays a clip, a blend of clips along a parameter, or nothing (the
// rest pose). It loops unless "loop" is false; "speed" is a number or a
// parameter. A blend plays the two points either side of the parameter's
// value - the nearest end alone outside them - on one phase, so a walk and
// a run half and half plant the same foot at the same moment. A state with
// "root_motion": true moves its entity as its clips' root went - a model
// imported with "root_motion" plays its clips on the spot, their travel
// kept apart -: the animator says how fast, and the script moves what it
// moves.
//
// Transitions are tried each fixed step, those from "*" - any state - first,
// then the current state's, each in file order; the first that holds is
// taken, and never into the state playing. "to": "return" goes back to the
// state the current one was entered from. "when" is a condition or a list
// of them, all of which must hold: "name", "!name", or "name <op> value" with
// ==, !=, <, <=, >, >= and a number, true or false. "exit_time" is a share
// of the clip the state must reach or pass in the step. "duration" is how
// long the pose eases into the new state, in seconds (0.2 if not said);
// while it does, only a transition with "interrupt": true is taken.
//
// "events" marks moments of clips, as shares of them, that the animator's
// entity hears through on_animation_event( self, name ) at the step they are
// passed - a footstep, the frame a swing lands. In a blend, the clip with
// the most weight is the one heard.
//
// "editor" holds what the graph's editor keeps - where it drew each state -
// and nothing here reads it.

enum class AnimationParameterKind : u8
{
    Number,
    Bool,
    Trigger,
};

struct AnimationParameter
{
    string mName;
    AnimationParameterKind mKind = AnimationParameterKind::Number;
    // A bool's or a trigger's 0 or 1.
    f32 mDefault = 0;
};

struct AnimationCondition
{
    enum class Op : u8
    {
        IsTrue,
        IsFalse,
        Equal,
        NotEqual,
        Less,
        LessOrEqual,
        Greater,
        GreaterOrEqual,
    };

    u32 mParameter = 0;
    Op mOp = Op::IsTrue;
    f32 mValue = 0;

    bool Holds( const vector<f32>& parameters ) const;
};

struct AnimationBlendPoint
{
    string mClip;
    f32 mValue = 0;
};

struct AnimationState
{
    string mName;
    // A clip, a blend - its points in order of their values -, or neither.
    string mClip;
    vector<AnimationBlendPoint> mBlend;
    u32 mBlendParameter = 0;
    bool mLoop = true;
    f32 mSpeed = 1;
    // Read instead of mSpeed, when there is one.
    opt<u32> mSpeedParameter;
    bool mRootMotion = false;
};

struct AnimationTransition
{
    // Nothing: from any state.
    opt<u32> mFrom;
    // Nothing: back to where the state was entered from.
    opt<u32> mTo;
    vector<AnimationCondition> mConditions;
    opt<f32> mExitTime;
    f32 mDuration = 0.2f;
    bool mInterrupt = false;
};

struct AnimationEvent
{
    // A share of the clip, 0 to 1.
    f32 mTime = 0;
    string mName;
};

// One point of a blend and how much it counts.
struct AnimationWeight
{
    u32 mPoint = 0;
    f32 mWeight = 0;
};

struct AnimationGraph
{
    vector<AnimationParameter> mParameters;
    vector<AnimationState> mStates;
    vector<AnimationTransition> mTransitions;
    u32 mEntry = 0;
    // By clip, each clip's in order of their times.
    hmap<string, vector<AnimationEvent>> mEvents;

    opt<u32> TryParameter( string_view name ) const;
    opt<u32> TryState( string_view name ) const;
    // Every clip the states name, each once, in the order they first come.
    vector<string> Clips() const;
};

constexpr string_view cAnimationGraphKind = "animation_graph";

// A .anim's text as a graph. The error says where, as Json does, and what is
// wrong: "4:20: states.hit: \"loop\" is true or false, not 1".
expected<AnimationGraph, string> TryParseAnimationGraph( string_view text );

// The one or two points of a blend that play when its parameter is `value`,
// their weights adding up to one; nothing for an empty blend.
vector<AnimationWeight> BlendWeights( const vector<AnimationBlendPoint>& points, f32 value );

// Where one animator is in its graph: what a fixed step changes, plain data
// a snapshot copies.
struct AnimationGraphState
{
    // By the graph's parameters; a bool or a trigger 0 or 1.
    vector<f32> mParameters;
    // -1 before the first step.
    i32 mState = -1;
    // The state the current one was entered from - where "return" goes.
    i32 mPrevious = -1;
    // How far through its clip the state is, 0 to 1: the step before, and now.
    f32 mPhaseBefore = 0;
    f32 mPhase = 0;
    // The last step went past the clip's end and around.
    bool mWrapped = false;
    // What is left of the easing into the state, in seconds.
    f32 mEasingLeft = 0;
    // Grows with each state entered - how the pose hears of a transition -
    // and the easing it asked for.
    u32 mEntered = 0;
    f32 mEnteredEasing = 0;
    // What the last step played, before a transition it took: the state, and
    // its phase from and to - what root motion is measured over.
    i32 mPlayed = -1;
    f32 mPlayedFrom = 0;
    f32 mPlayedTo = 0;
    bool mPlayedWrapped = false;
};

// Before the first step: every parameter at its default.
AnimationGraphState StartAnimationGraph( const AnimationGraph& graph );

// The clip of a state with the most weight now: what its events are of.
// Empty for a state that plays nothing.
string_view LeadingClip( const AnimationGraph& graph, const AnimationGraphState& state );

// One fixed step of `dt` at the animator's `speed`: the state's clip goes on
// - its length, or its blend's, from `durationOf` (a clip it cannot find
// lasts a second) -, the events it passes are appended to `events`, then the
// first transition that holds is taken, and the triggers are cleared.
void StepAnimationGraph( const AnimationGraph& graph,
                         AnimationGraphState& state,
                         f32 dt,
                         f32 speed,
                         const function<opt<f32>( string_view clip )>& durationOf,
                         vector<string>& events );
}
