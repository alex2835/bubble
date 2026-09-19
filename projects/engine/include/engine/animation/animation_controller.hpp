#pragma once
#include "engine/types/number.hpp"
#include "engine/types/string.hpp"
#include "engine/types/array.hpp"
#include "engine/types/map.hpp"
#include "engine/types/json.hpp"
#include "engine/utils/filesystem.hpp"
#include "engine/animation/blend_space.hpp"
#include <optional>

namespace bubble
{
// An animation controller: the states a character can be in, what each one
// plays, and the rules for moving between them. A JSON asset next to the
// model, evaluated every frame against a set of parameters a script writes.
//
// The script never picks a clip. It says speed = 3.2 or trigger( "attack" ),
// and the controller decides what that means for this character - which is
// what lets two characters with the same clip names share one controller, and
// what keeps the animation logic out of the gameplay code.
//
//   {
//     "parameters": { "speed": 0, "grounded": true, "attack": "trigger" },
//     "entry": "locomotion",
//     "states": {
//       "locomotion": { "blend": { "param": "speed",
//                                  "points": [ ["idle", 0], ["walk", 1.5], ["run", 4] ] } },
//       "jump":       { "clip": "jump", "loop": false },
//       "attack":     { "clip": "attack_1", "loop": false, "speed": "attack_speed" }
//     },
//     "transitions": [
//       { "from": "*",          "to": "jump",       "when": "!grounded",      "duration": 0.1 },
//       { "from": "jump",       "to": "locomotion", "when": "grounded",       "duration": 0.2 },
//       { "from": "locomotion", "to": "attack",     "when": "attack",         "duration": 0.05 },
//       { "from": "attack",     "to": "return",     "exit_time": 0.9,         "duration": 0.2 }
//     ]
//   }
//
// Parameters are float, bool or trigger. A trigger is a bool a script sets
// and the controller clears: the transition that fires on it consumes it,
// and one nothing fired on is dropped at the end of the frame - a queued
// attack does not go off two seconds later.
//
// "from": "*" is any state, checked before the current state's own
// transitions, and never into the state already playing. "to": "return" goes
// back to the state the current one was entered from - one edge out of a
// one-shot instead of one per state it might have interrupted.
//
// A condition is "name", "!name", or "name <op> value" with ==, !=, <, <=,
// >, >=; several are ANDed. "exit_time" is a normalized time the state must
// have reached (or passed this frame, looping). "interrupt" lets a transition
// fire while the pose is still easing into the current state; off by
// default, so a chain of transitions in one frame cannot flicker through
// three states.

struct Parameter
{
    enum class Type { Float, Bool, Trigger };
    Type mType = Type::Float;
    f32 mValue = 0.0f; // bool and trigger: 0 or 1

    bool operator==( const Parameter& ) const = default;
};


// The live values a controller reads, keyed by name. A script writes them;
// a missing name is created as a float, so a script and a controller can be
// written in either order.
struct Parameters
{
    void Set( string_view name, f32 value );
    void Set( string_view name, bool value );
    void Trigger( string_view name );
    f32 Get( string_view name ) const;
    bool Has( string_view name ) const { return mValues.contains( name ); }
    void ResetTriggers();

    str_hash_map<Parameter> mValues;
};


struct Condition
{
    enum class Op { IsTrue, IsFalse, Eq, Ne, Lt, Le, Gt, Ge };
    string mParameter;
    Op mOp = Op::IsTrue;
    f32 mValue = 0.0f;

    bool Holds( const Parameters& parameters ) const;
    // "speed > 1.5", "grounded", "!grounded", "grounded == false".
    static Condition Parse( string_view text );
    string ToString() const;
};


struct ControllerState
{
    string mName;
    // One or the other.
    string mClip;
    BlendSpace mBlend;
    string mBlendParameter;

    bool mLoop = true;
    f32 mSpeed = 1.0f;
    // Read instead of mSpeed when set.
    string mSpeedParameter;

    bool IsBlend() const { return not mBlend.Empty(); }
};


struct Transition
{
    static constexpr i32 cAnyState = -1;
    static constexpr i32 cReturn = -1;

    i32 mFrom = cAnyState;
    i32 mTo = cReturn;
    vector<Condition> mConditions;
    std::optional<f32> mExitTime;
    f32 mDuration = 0.2f;
    bool mInterrupt = false;
};


struct AnimationController
{
    string mName;
    path mPath;
    // In file order, for the inspector.
    vector<std::pair<string, Parameter>> mParameters;
    vector<ControllerState> mStates;
    vector<Transition> mTransitions;
    i32 mEntry = 0;

    // Throws std::runtime_error with what is wrong and where.
    static AnimationController FromJson( const json& json, const path& source );
    i32 FindState( string_view name ) const;
    // A Parameters with every declared parameter at its default.
    Parameters DefaultParameters() const;
};


// Where a controller is, for one entity. Pure over the controller and the
// parameters, so it runs without a model in the tests.
struct ControllerRuntime
{
    i32 mCurrent = -1;
    i32 mPrevious = -1;

    struct Frame
    {
        f32 mNormalizedTime = 0.0f;
        f32 mPreviousNormalizedTime = 0.0f;
        // The playback wrapped past the end this frame.
        bool mWrapped = false;
        bool mInTransition = false;
    };

    struct Change
    {
        i32 mState = -1;
        f32 mDuration = 0.0f;
    };

    // Enters the entry state, or the state `state`; the change to play.
    Change Enter( const AnimationController& controller, i32 state, f32 duration );
    // Takes the first transition whose conditions hold, if any: consumes its
    // triggers and moves; the change to play.
    std::optional<Change> Step( const AnimationController& controller,
                                Parameters& parameters,
                                const Frame& frame );
    // Whether `transition` would fire now; what the inspector shows.
    bool Satisfied( const AnimationController& controller,
                    const Transition& transition,
                    const Parameters& parameters,
                    const Frame& frame ) const;
};

}
