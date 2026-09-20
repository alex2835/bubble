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
//
// "events" marks moments in clips, by normalized time, that a script hears
// about through animator:events() - footsteps, the frame a swing connects:
//
//     "events": { "walk": [ [0.32, "footstep"], [0.82, "footstep"] ] }
//
// In a blend, the clip with the most weight is the one whose events fire.
//
// "layers" are state machines of their own played over the base one on a
// part of the skeleton - an aim or a wave on the upper body while the legs
// keep walking:
//
//     "layers": [ { "name": "upper", "mask": [ "mixamorig:Spine1" ], "weight": 1,
//                   "entry": "none",
//                   "states": { "none": {}, "wave": { "clip": "wave", "loop": false } },
//                   "transitions": [ ... ] } ]
//
// The mask is a list of joints, each taken with everything below it; a name
// with a leading "!" takes its subtree back out. "weight" is a number or a
// parameter. A state with neither clip nor blend plays nothing: on the base
// that is the rest pose, on a layer it is the layer fading out, over the
// transition's duration. Layers share the parameters with the base.
//
// "root_joint" names the joint - the hips, usually - whose horizontal travel
// and yaw a state with "root_motion": true takes out of its clips: the
// character animates on the spot, and the movement it would have made is
// read each frame with animator:root_delta() and applied by the script to
// whatever moves the entity. That keeps the animation from moving a body the
// physics owns.
//
// An "additive": true layer adds its clips on top of the pose instead of
// replacing it, each clip as the change from its own first frame - so a
// lean or a hit reaction is authored from a neutral first frame and lands
// on whatever the character is doing.

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


// A moment in a clip, at a normalized time.
struct ClipEvent
{
    f32 mTime = 0.0f;
    string mName;
};
using ClipEvents = str_hash_map<vector<ClipEvent>>;

// The events of `clip` crossed by a playback that went from `before` to
// `now`, in that order - wrapping around the end if `wrapped`, running
// backwards if `now` < `before` without a wrap. Sorted markers in, names
// out, in the order they were passed.
void CrossedEvents( const ClipEvents& events, string_view clip,
                    f32 before, f32 now, bool wrapped, vector<string>& out );


struct ControllerState
{
    string mName;
    // One, the other, or neither.
    string mClip;
    BlendSpace mBlend;
    string mBlendParameter;

    bool mLoop = true;
    f32 mSpeed = 1.0f;
    // Read instead of mSpeed when set.
    string mSpeedParameter;
    // The root's travel comes out of the clips and into root_delta().
    bool mRootMotion = false;

    bool IsBlend() const { return not mBlend.Empty(); }
    bool IsEmpty() const { return mClip.empty() and mBlend.Empty(); }
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


// States, transitions, and where to start: the base machine, or a layer's.
struct StateMachine
{
    vector<ControllerState> mStates;
    vector<Transition> mTransitions;
    i32 mEntry = 0;

    i32 FindState( string_view name ) const;
};


// A state machine played over the base on part of the skeleton.
struct ControllerLayer
{
    string mName;
    vector<string> mMask;
    f32 mWeight = 1.0f;
    string mWeightParameter;
    bool mAdditive = false;
    StateMachine mMachine;
};


struct AnimationController : StateMachine
{
    string mName;
    path mPath;
    // In file order, for the inspector.
    vector<std::pair<string, Parameter>> mParameters;
    ClipEvents mEvents;
    vector<ControllerLayer> mLayers;
    // The joint whose travel is root motion, for states that ask for it.
    string mRootJoint;
    // The graph window's layout, kept in the file under "editor" so a save
    // round trips it: a node position per state, keyed "state" for the base
    // machine and "layer/state" for a layer's. Nothing at runtime reads it.
    map<string, vec2> mNodePositions;

    // Throws std::runtime_error with what is wrong and where.
    static AnimationController FromJson( const json& json, const path& source );
    // The same file back: what FromJson reads, plus the layout.
    json ToJson() const;
    // ToJson to mPath, pretty printed. False, with the error logged, if the
    // file could not be written.
    bool Save() const;
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
    Change Enter( const StateMachine& machine, i32 state, f32 duration );
    // Takes the first transition whose conditions hold, if any: consumes its
    // triggers and moves; the change to play.
    std::optional<Change> Step( const StateMachine& machine,
                                Parameters& parameters,
                                const Frame& frame );
    // Whether `transition` would fire now; what the inspector shows.
    bool Satisfied( const StateMachine& machine,
                    const Transition& transition,
                    const Parameters& parameters,
                    const Frame& frame ) const;
};

}
