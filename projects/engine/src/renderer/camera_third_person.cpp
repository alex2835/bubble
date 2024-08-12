
#include "engine/renderer/camera_third_person.hpp"

namespace bubble
{

ThirdPersonCamera::ThirdPersonCamera( f32 yaw, f32 pitch )
    : mCenter( vec3( 0, 0, 0 ) ),
      Camera( vec3(), yaw, pitch )
{
}

void ThirdPersonCamera::ProcessRotation( CameraMovement direction )
{
    f32 max_speed = mMaxSpeed * mDeltaSpeed;

    // Horizontal speed
    if( direction == CameraMovement::LEFT )
    {
        if( mSpeedX < 0 )
            mSpeedX = 0;
        mSpeedX = mSpeedX < max_speed ? mSpeedX + mDeltaSpeed : max_speed;
        mIsRotatingX = true;
    }
    else if( direction == CameraMovement::RIGHT )
    {
        if( mSpeedX > 0 )
            mSpeedX = 0;
        mSpeedX = mSpeedX > -max_speed ? mSpeedX - mDeltaSpeed : -max_speed;
        mIsRotatingX = true;
    }

    // Vertical speed
    if( direction == CameraMovement::UP )
    {
        if( mSpeedY < 0 )
            mSpeedY = 0;
        mSpeedY = mSpeedY < max_speed ? mSpeedY + mDeltaSpeed : max_speed;
        mIsRotatingY = true;
    }
    else if( direction == CameraMovement::DOWN )
    {
        if( mSpeedY > 0 )
            mSpeedY = 0;
        mSpeedY = mSpeedY > -max_speed ? mSpeedY - mDeltaSpeed : -max_speed;
        mIsRotatingY = true;
    }

    // Clamp
    if( std::fabs( mSpeedX ) > max_speed )
        mSpeedX = Sign( mSpeedX ) * max_speed;

    if( std::fabs( mSpeedY ) > max_speed )
        mSpeedY = Sign( mSpeedY ) * max_speed;
}


void ThirdPersonCamera::ProcessMouseMovement( f32 MousePosX, f32 MousePosY )
{
    f32 xoffset = ( mLastMouseX - MousePosX ) * mMouseSensitivity;
    f32 yoffset = ( mLastMouseY - MousePosY ) * mMouseSensitivity;

    mLastMouseX = MousePosX;
    mLastMouseY = MousePosY;

    mYaw -= xoffset;
    mPitch -= yoffset;

    if( mPitch > camera::PI / 2.0f - 0.1f )
        mPitch = camera::PI / 2.0f - 0.1f;

    if( mPitch < -camera::PI / 2.0f + 0.1f )
        mPitch = -camera::PI / 2.0f + 0.1f;
}


void ThirdPersonCamera::ProcessMouseMovementOffset( f32 xoffset, f32 yoffset )
{
    xoffset *= mMouseSensitivity;
    yoffset *= mMouseSensitivity;

    mYaw += xoffset;
    mPitch += yoffset;

    if( mPitch > camera::PI / 2.0f - 0.1f )
        mPitch = camera::PI / 2.0f - 0.1f;

    if( mPitch < -camera::PI / 2.0f + 0.1f )
        mPitch = -camera::PI / 2.0f + 0.1f;
}


void ThirdPersonCamera::ProcessMouseScroll( f32 yoffset )
{
    if( mFov >= 0.1f && mFov <= camera::PI / 2.0f )
        mFov -= yoffset * mDeltaFov;

    if( mFov < 0.1f )
        mFov = 0.1f;

    if( mFov > camera::PI / 2.0f )
        mFov = camera::PI / 2.0f;
}


void ThirdPersonCamera::OnUpdate( DeltaTime dt )
{
    // Inertia
    if( !mIsRotatingX )
        mSpeedX = std::abs( mSpeedX ) < 0.01f ? mSpeedX = 0 : mSpeedX - Sign( mSpeedX ) * mDeltaSpeed;

    if( !mIsRotatingY )
        mSpeedY = std::abs( mSpeedY ) < 0.01f ? mSpeedY = 0 : mSpeedY - Sign( mSpeedY ) * mDeltaSpeed;

    mIsRotatingX = false;
    mIsRotatingY = false;

    mYaw += mSpeedX * dt;
    mPitch += mSpeedY * dt;

    // Transformation matrix
    mat4 transform = mat4( 1.0f );
    transform = rotate( transform, mYaw, vec3( 0, 1, 0 ) );
    transform = rotate( transform, mPitch, vec3( 1, 0, 0 ) );
    transform = translate( transform, mCenter );

    mPosition = transform * vec4( 0, 0, mRadius, 0 );

    // Basis
    mFront = normalize( mCenter - mPosition );
    mRight = normalize( cross( mFront, mWorldUp ) );
    mUp = normalize( cross( mRight, mFront ) );
}

}