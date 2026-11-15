#include "bubble/input/input.hpp"
#include "bubble/types/algorithm.hpp"
#include "bubble/types/format.hpp"

namespace bubble
{
namespace
{
// What is left of an axis past the dead zone, scaled to go from 0 to 1
// again either side.
f32 PastDeadZone( f32 value )
{
    const f32 size = abs( value );
    if ( size <= Input::cDeadZone )
        return 0.0f;
    const f32 past = min( ( size - Input::cDeadZone ) / ( 1.0f - Input::cDeadZone ), 1.0f );
    return value < 0.0f ? -past : past;
}

bool Inside( const TouchArea& area, vec2 position )
{
    return position.x >= area.mMin.x and position.x <= area.mMax.x and position.y >= area.mMin.y and
           position.y <= area.mMax.y;
}
}

Actions::Actions( Ref<const ActionMap> map, vector<ActionState> states )
    : mMap( std::move( map ) ),
      mStates( std::move( states ) )
{
}

const ActionState& Actions::State( Name action ) const
{
    const opt<u32> indexMaybe = mMap ? mMap->TryIndexOf( action ) : nullopt;
    if ( not indexMaybe )
        throw logic_error( format( "no action \"{}\" ({})", action.ToString(),
                                   mMap and not mMap->List().empty() ? "actions: " + mMap->Names() : "no actions" ) );
    return mStates[*indexMaybe];
}

Input::Input( ActionMap map ) : mMap( CreateRef<const ActionMap>( std::move( map ) ) )
{
    const size_t count = mMap->List().size();
    mValues.assign( count, 0.0f );
    mDown.assign( count, false );
    mStepEdges = Edges{ .mPressed = vector<bool>( count ), .mReleased = vector<bool>( count ) };
    mFrameEdges = mStepEdges;
}

void Input::Handle( const InputEvent& event )
{
    if ( const auto* keyDown = get_if<KeyDown>( &event ) )
        mKeys.at( static_cast<u32>( keyDown->mKey ) ) = true;
    else if ( const auto* keyUp = get_if<KeyUp>( &event ) )
        mKeys.at( static_cast<u32>( keyUp->mKey ) ) = false;
    else if ( const auto* mouseDown = get_if<MouseDown>( &event ) )
        mMouse.at( static_cast<u32>( mouseDown->mButton ) ) = true;
    else if ( const auto* mouseUp = get_if<MouseUp>( &event ) )
        mMouse.at( static_cast<u32>( mouseUp->mButton ) ) = false;
    else if ( const auto* touchDown = get_if<TouchDown>( &event ) )
        mTouches[touchDown->mFinger] = touchDown->mPosition;
    else if ( const auto* touchUp = get_if<TouchUp>( &event ) )
        mTouches.erase( touchUp->mFinger );
    else if ( const auto* added = get_if<GamepadAdded>( &event ) )
        mGamepads.try_emplace( added->mGamepad );
    // Gone with whatever it held.
    else if ( const auto* removed = get_if<GamepadRemoved>( &event ) )
        mGamepads.erase( removed->mGamepad );
    else if ( const auto* buttonDown = get_if<GamepadButtonDown>( &event ) )
        mGamepads[buttonDown->mGamepad].mButtons.at( static_cast<u32>( buttonDown->mButton ) ) = true;
    else if ( const auto* buttonUp = get_if<GamepadButtonUp>( &event ) )
        mGamepads[buttonUp->mGamepad].mButtons.at( static_cast<u32>( buttonUp->mButton ) ) = false;
    else if ( const auto* axis = get_if<GamepadAxisMove>( &event ) )
        mGamepads[axis->mGamepad].mAxes.at( static_cast<u32>( axis->mAxis ) ) = axis->mValue;
    // A move or the wheel drives no action: a touch holds the areas it
    // started in.
    else
        return;
    Update();
}

Actions Input::LatchStep()
{
    return Latch( mStepEdges );
}

Actions Input::LatchFrame()
{
    return Latch( mFrameEdges );
}

f32 Input::ValueOf( const ActionInput& input ) const
{
    if ( const auto* key = get_if<Key>( &input ) )
        return mKeys.at( static_cast<u32>( *key ) ) ? 1.0f : 0.0f;
    if ( const auto* button = get_if<MouseButton>( &input ) )
        return mMouse.at( static_cast<u32>( *button ) ) ? 1.0f : 0.0f;
    if ( const auto* padButton = get_if<GamepadButton>( &input ) )
    {
        for ( const auto& [id, gamepad] : mGamepads )
            if ( gamepad.mButtons.at( static_cast<u32>( *padButton ) ) )
                return 1.0f;
        return 0.0f;
    }
    if ( const auto* axis = get_if<GamepadAxis>( &input ) )
    {
        // Of several pads, the one pushed furthest.
        f32 furthest = 0.0f;
        for ( const auto& [id, gamepad] : mGamepads )
        {
            const f32 value = gamepad.mAxes.at( static_cast<u32>( *axis ) );
            if ( abs( value ) > abs( furthest ) )
                furthest = value;
        }
        return PastDeadZone( furthest );
    }
    const TouchArea& area = get<TouchArea>( input );
    for ( const auto& [finger, start] : mTouches )
        if ( Inside( area, start ) )
            return 1.0f;
    return 0.0f;
}

void Input::Update()
{
    const span<const Action> actions = mMap->List();
    for ( size_t i = 0; i < actions.size(); ++i )
    {
        f32 value = 0.0f;
        for ( const ActionBinding& binding : actions[i].mBindings )
            value += ValueOf( binding.mInput ) * binding.mScale;
        mValues[i] = clamp( value, -1.0f, 1.0f );
        const bool down = abs( mValues[i] ) >= 0.5f;
        if ( down == mDown[i] )
            continue;
        mDown[i] = down;
        ( down ? mStepEdges.mPressed : mStepEdges.mReleased )[i] = true;
        ( down ? mFrameEdges.mPressed : mFrameEdges.mReleased )[i] = true;
    }
}

Actions Input::Latch( Edges& edges )
{
    vector<ActionState> states( mValues.size() );
    for ( size_t i = 0; i < states.size(); ++i )
    {
        states[i] = ActionState{
            .mValue = mValues[i], .mDown = mDown[i], .mPressed = edges.mPressed[i], .mReleased = edges.mReleased[i]
        };
        edges.mPressed[i] = false;
        edges.mReleased[i] = false;
    }
    return Actions( mMap, std::move( states ) );
}
}
