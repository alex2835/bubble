#pragma once
#include "bubble/input/action_map.hpp"
#include "bubble/input/input_event.hpp"
#include "bubble/types/pointer.hpp"

namespace bubble
{
// One action as a fixed step - or a frame - sees it.
struct ActionState
{
    // Where the action stood when the step began, from -1 to 1: buttons 0 or
    // 1, sticks between. The bindings' values add up, so opposite keys
    // cancel.
    f32 mValue = 0.0f;
    // At least half way, either side: held.
    bool mDown = false;
    // Went down since the step before - seen by this step alone, even when
    // it went up again before the step began: a tap between two steps.
    bool mPressed = false;
    bool mReleased = false;

    friend bool operator==( const ActionState&, const ActionState& ) = default;
};

// The actions as one fixed step or one frame sees them: what a script's
// input.pressed( "jump" ) will read, and what a replay records. An action
// the map does not have is a logic_error naming those it does.
class Actions
{
public:
    // None at all: a world nobody plays.
    Actions() = default;

    bool Down( Name action ) const { return State( action ).mDown; }
    bool Pressed( Name action ) const { return State( action ).mPressed; }
    bool Released( Name action ) const { return State( action ).mReleased; }
    f32 Axis( Name action ) const { return State( action ).mValue; }
    const ActionState& State( Name action ) const;
    // The same for a name from a script: an action the map does not have
    // is an error naming those it does.
    expected<ActionState, string> TryState( Name action ) const;
    // Whether a map made them: a world nobody plays has none.
    bool Mapped() const { return mMap != nullptr; }

    // In the map's order.
    span<const ActionState> States() const { return mStates; }

private:
    friend class Input;
    Actions( Ref<const ActionMap> map, vector<ActionState> states );

    Ref<const ActionMap> mMap;
    vector<ActionState> mStates;
};

// The devices as the events left them - keys and buttons held, sticks,
// touches - and the actions the map makes of them. Events go in, after the
// UI had its turn; what fixed steps and frames see comes out latched: a
// press is seen by exactly one fixed step, and by exactly one frame, at
// any frame rate.
class Input
{
public:
    explicit Input( ActionMap map );

    void Handle( const InputEvent& event );

    // What a fixed step sees, taken as it begins: the actions now, and what
    // was pressed and released since the step before. A press while no step
    // runs waits for the next one; of a frame's several steps, the first
    // sees it.
    Actions LatchStep();
    // The same for a frame - its Update, the camera - kept apart from the
    // steps': a press is a frame's once and a step's once.
    Actions LatchFrame();

    const ActionMap& Map() const { return *mMap; }

    // Below this a stick's axis is at rest, and above it the axis is scaled
    // to start from 0 again: a worn stick does not drift.
    static constexpr f32 cDeadZone = 0.2f;

private:
    struct Gamepad
    {
        array<bool, cGamepadButtons> mButtons{};
        array<f32, cGamepadAxes> mAxes{};
    };
    // What each action did since a latch last took it.
    struct Edges
    {
        vector<bool> mPressed;
        vector<bool> mReleased;
    };

    f32 ValueOf( const ActionInput& input ) const;
    void Update();
    Actions Latch( Edges& edges );

    Ref<const ActionMap> mMap;
    array<bool, cKeySlots> mKeys{};
    array<bool, cMouseButtons> mMouse{};
    hmap<u32, Gamepad> mGamepads;
    // Where each finger down started: which areas it holds.
    hmap<u64, vec2> mTouches;
    vector<f32> mValues;
    vector<bool> mDown;
    Edges mStepEdges;
    Edges mFrameEdges;
};
}
