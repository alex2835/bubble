#include "engine/pch/pch.hpp"
#include "engine/scene/components/character_controller_component.hpp"
#include "engine/reflection/reflection.hpp"
#include "engine/types/glm.hpp"
#include <sol/sol.hpp>

namespace bubble
{
namespace
{
void Changed( CharacterControllerComponent& c )
{
    c.Apply();
}

bool InWorld( const CharacterController& controller )
{
    return controller.GetGhostObject()->getBroadphaseHandle() != nullptr;
}
}

void CharacterControllerComponent::Reflect()
{
    TypeBuilder<CharacterControllerComponent>( Name().data() )
        .Field<&CharacterControllerComponent::mRadius>( "radius", { .mMin = 0.1f, .mMax = 10.0f, .mSpeed = 0.01f } )
        .Field<&CharacterControllerComponent::mHeight>( "height", {
            .mMin = 0.0f, .mMax = 10.0f, .mSpeed = 0.01f,
            .mTooltip = "Between the two caps: the whole capsule is height + 2 * radius." } )
        .Field<&CharacterControllerComponent::mStepHeight>( "step_height", { .mMin = 0.0f, .mMax = 2.0f, .mSpeed = 0.01f } )
        .Field<&CharacterControllerComponent::mMaxSlope>( "max_slope", {
            .mMin = 0.0f, .mMax = 90.0f, .mFlags = FieldInfo::Angle | FieldInfo::Slider } )
        .Field<&CharacterControllerComponent::mJumpSpeed>( "jump_speed", { .mMin = 0.0f, .mMax = 50.0f, .mSpeed = 0.1f } )
        .Field<&CharacterControllerComponent::mFallSpeed>( "fall_speed", { .mMin = 0.0f, .mMax = 100.0f, .mSpeed = 0.1f } )
        .Field<&CharacterControllerComponent::mGravity>( "gravity", { .mSpeed = 0.1f } )
        .OnChanged<&Changed>();
}

void CharacterControllerComponent::BindLuaMethods( sol::state&, sol::usertype<CharacterControllerComponent>& type )
{
    // Units per second: what movement code should use.
    type["set_walk_velocity"] = []( CharacterControllerComponent& c, const vec3& v ) { c.mController.SetWalkVelocity( v ); };
    // Bullet's raw form: a displacement per physics substep, not a velocity.
    type["set_walk_direction"] = []( CharacterControllerComponent& c, const vec3& d ) { c.mController.SetWalkDirection( d ); };
    type["set_velocity_for_time_interval"] = []( CharacterControllerComponent& c, const vec3& v, f32 t )
    {
        c.mController.SetVelocityForTimeInterval( v, t );
    };
    // Unconditional: the script decides whether a jump is allowed.
    type["jump"] = sol::overload(
        []( CharacterControllerComponent& c ) { c.mController.Jump(); },
        []( CharacterControllerComponent& c, const vec3& direction ) { c.mController.Jump( direction ); } );
    type["warp"] = []( CharacterControllerComponent& c, const vec3& position ) { c.mController.Warp( position ); };
    type["is_on_ground"] = []( const CharacterControllerComponent& c ) { return c.mController.IsOnGround(); };
    type["get_position"] = []( const CharacterControllerComponent& c ) { return c.mController.GetPosition(); };
    type["get_linear_velocity"] = []( const CharacterControllerComponent& c ) { return c.mController.GetLinearVelocity(); };
    type["set_max_jump_height"] = []( CharacterControllerComponent& c, f32 h ) { c.mController.SetMaxJumpHeight( h ); };
}

CharacterControllerComponent::CharacterControllerComponent()
    : CharacterControllerComponent( 0.5f, 1.0f, 0.35f )
{
}

CharacterControllerComponent::CharacterControllerComponent( f32 radius, f32 height, f32 stepHeight )
    : mRadius( radius ),
      mHeight( height ),
      mStepHeight( stepHeight ),
      mController( radius, height, stepHeight )
{
    Apply();
}

CharacterControllerComponent::~CharacterControllerComponent()
{
}

void CharacterControllerComponent::Apply()
{
    const CharacterController& c = mController;
    if ( c.GetRadius() != mRadius or c.GetHeight() != mHeight or c.GetStepHeight() != mStepHeight )
    {
        if ( InWorld( c ) )
            mRebuild = true;
        else
            Rebuild();
        return;
    }
    // Only what changed: Bullet's jump speed is also where jump() keeps the
    // speed of the jump under way, and caps the climb at it every step. A
    // script that sets gravity each frame reset it to jump_speed, and a jump
    // of 33 rose at 10.
    if ( c.GetJumpSpeed() != mJumpSpeed )
        mController.SetJumpSpeed( mJumpSpeed );
    if ( c.GetFallSpeed() != mFallSpeed )
        mController.SetFallSpeed( mFallSpeed );
    if ( c.GetMaxSlopeRadians() != mMaxSlope )
        mController.SetMaxSlope( mMaxSlope );
    if ( c.GetGravity() != mGravity )
        mController.SetGravity( mGravity );
}

void CharacterControllerComponent::Rebuild()
{
    const vec3 position = mController.GetPosition();
    mController = CharacterController( mRadius, mHeight, mStepHeight );
    mController.Warp( position );
    mController.SetJumpSpeed( mJumpSpeed );
    mController.SetFallSpeed( mFallSpeed );
    mController.SetMaxSlope( mMaxSlope );
    mController.SetGravity( mGravity );
    mRebuild = false;
}

}
