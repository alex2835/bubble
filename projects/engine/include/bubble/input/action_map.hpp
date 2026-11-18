#pragma once
#include "bubble/core/name.hpp"
#include "bubble/input/controls.hpp"
#include "bubble/types/containers.hpp"
#include "bubble/types/math.hpp"
#include "bubble/types/variant.hpp"

namespace bubble
{
// An area of the window as shares of its size, (0, 0) top left: a touch
// that starts in it holds the action until the finger lifts, wherever it
// goes meanwhile - an on-screen button.
struct TouchArea
{
    vec2 mMin{ 0.0f };
    vec2 mMax{ 1.0f };

    friend bool operator==( const TouchArea&, const TouchArea& ) = default;
};

// What can drive an action. A mouse axis gives how far the mouse went since
// the step or the frame before - times the binding's scale, never clamped:
// a camera turns by it, where a stick's value is a rate.
using ActionInput = variant<Key, MouseButton, GamepadButton, GamepadAxis, TouchArea, MouseAxis>;

struct ActionBinding
{
    ActionInput mInput;
    // What the input adds to the action: -1 makes a key the negative side of
    // an axis - A -1 and D 1 on "move_x" - or turns a stick's axis round.
    f32 mScale = 1.0f;
};

struct Action
{
    Name mName;
    vector<ActionBinding> mBindings;
};

// The actions a game reads - "jump", "move_x" - and what drives each: keys,
// mouse and gamepad buttons, sticks, touches. A game reads actions, never
// keys, so a binding changes in one place and a phone plays what a
// keyboard does.
class ActionMap
{
public:
    // Binds `input` to the action, made if it is new; actions keep the order
    // they were first bound in. A name that is not snake_case is a
    // logic_error.
    ActionMap& Bind( string_view action, ActionInput input, f32 scale = 1.0f );

    span<const Action> List() const { return mActions; }
    // Where the action is in List().
    opt<u32> TryIndexOf( Name action ) const;
    // "jump, move_x", for a message naming what there is.
    string Names() const;

private:
    vector<Action> mActions;
    hmap<Name, u32> mIndices;
};
}
