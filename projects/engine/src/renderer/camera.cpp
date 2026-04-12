
#include "engine/renderer/camera.hpp"

namespace bubble
{
Camera::Camera( vec3 position, f32 yaw, f32 pitch, f32 fov, vec3 up )
    : mPosition( position ),
      mWorldUp( up ),
      mFov( fov ),
      mYaw( yaw ),
      mPitch( pitch )
{}

mat4 Camera::GetLookatMat() const
{
    return lookAt( mPosition, mPosition + mForward, mUp );
}

mat4 Camera::GetProjectionMat( i32 window_width, i32 window_height ) const
{
    f32 aspect = (f32)window_width / window_height;
    return glm::perspective<f32>( mFov, aspect, mNear, mFar );
}


// Process mouse

void Camera::ProcessMouseMovement( f32 mousePosX, f32 mousePosY )
{
    f32 xoffset = ( mLastMouseX - mousePosX ) * mMouseSensitivity;
    f32 yoffset = ( mLastMouseY - mousePosY ) * mMouseSensitivity;

    mLastMouseX = mousePosX;
    mLastMouseY = mousePosY;

    mYaw -= xoffset;
    mPitch -= yoffset;

    if ( mPitch > camera::PI / 2.0f - 0.1f )
        mPitch = camera::PI / 2.0f - 0.1f;

    if ( mPitch < -camera::PI / 2.0f + 0.1f )
        mPitch = -camera::PI / 2.0f + 0.1f;
}

void Camera::ProcessMouseMovementOffset( f32 xoffset, f32 yoffset )
{
    xoffset *= mMouseSensitivity;
    yoffset *= mMouseSensitivity;

    mYaw += xoffset;
    mPitch += yoffset;

    if ( mPitch > camera::PI / 2.0f - 0.1f )
        mPitch = camera::PI / 2.0f - 0.1f;

    if ( mPitch < -camera::PI / 2.0f + 0.1f )
        mPitch = -camera::PI / 2.0f + 0.1f;
}

void Camera::ProcessMouseScroll( f32 offset )
{
    if ( mFov >= 0.1f && mFov <= camera::PI / 2.0f )
        mFov += offset * mDeltaFov;

    if ( mFov < 0.1f )
        mFov = 0.1f;

    if ( mFov > camera::PI / 2.0f )
        mFov = camera::PI / 2.0f;
}


// Free camera 

void Camera::ProcessMovement( float dt, CameraMovement direction )
{
    f32 max_speed = mMaxSpeed * mDeltaSpeed;

    // Speed forward
    if ( direction == CameraMovement::FORWARD )
    {
        if ( mSpeedForwardOrUp < 0 )
            mSpeedForwardOrUp *= 0.5f;
        mSpeedForwardOrUp = mSpeedForwardOrUp + mDeltaSpeed * dt;
        mIsMovingForward = true;
    }
    else if ( direction == CameraMovement::BACKWARD )
    {
        if ( mSpeedForwardOrUp > 0 )
            mSpeedForwardOrUp *= 0.5f;
        mSpeedForwardOrUp = mSpeedForwardOrUp - mDeltaSpeed * dt;
        mIsMovingForward = true;
    }

    // Speed right
    if ( direction == CameraMovement::RIGHT )
    {
        if ( mSpeedRight < 0 )
            mSpeedRight *= 0.5f;
        mSpeedRight = mSpeedRight + mDeltaSpeed * dt;
        mIsMovingRight = true;
    }
    else if ( direction == CameraMovement::LEFT )
    {
        if ( mSpeedRight > 0 )
            mSpeedRight *= 0.5f;
        mSpeedRight = mSpeedRight - mDeltaSpeed * dt;
        mIsMovingRight = true;
    }

    // Clamp
    mSpeedForwardOrUp = std::clamp( mSpeedForwardOrUp, -max_speed, max_speed );
    mSpeedRight = std::clamp( mSpeedRight, -max_speed, max_speed );
}

void Camera::EulerAnglesToVectors()
{
    vec3 forward;
    forward.x = std::cos( mYaw ) * std::cos( mPitch );
    forward.y = std::sin( mPitch );
    forward.z = std::sin( mYaw ) * std::cos( mPitch );

    mForward = normalize( forward );
    mRight = normalize( cross( mForward, mWorldUp ) );
    mUp = normalize( cross( mRight, mForward ) );
}

void Camera::OnUpdateFreeCamera( float dt )
{
    // Inertia
    if ( !mIsMovingForward )
    {
        if ( std::abs( mSpeedForwardOrUp ) < 0.1f * mDeltaSpeed )
            mSpeedForwardOrUp = 0;
        else
            mSpeedForwardOrUp = mSpeedForwardOrUp - Sign( mSpeedForwardOrUp ) * mDeltaSpeed * dt;
    }

    if ( !mIsMovingRight )
    {
        if ( std::abs( mSpeedRight ) < 0.1f * mDeltaSpeed )
            mSpeedRight = 0;
        else
            mSpeedRight = mSpeedRight - Sign( mSpeedRight ) * mDeltaSpeed * dt;
    }

    mIsMovingForward = false;
    mIsMovingRight = false;

    mPosition += mForward * mSpeedForwardOrUp * dt;
    mPosition += mRight * mSpeedRight * dt;

    EulerAnglesToVectors();
}




// Third person camera 

void Camera::ProcessThirdPersonRotation( CameraMovement direction )
{
    f32 max_speed = mMaxSpeed * mDeltaSpeed;

    // Horizontal speed (yaw) — guarded by mIsRotatingUp in OnUpdateThirdPerson
    if ( direction == CameraMovement::LEFT )
    {
        if ( mSpeedForwardOrUp < 0 )
            mSpeedForwardOrUp = 0;
        mSpeedForwardOrUp = mSpeedForwardOrUp < max_speed ? mSpeedForwardOrUp + mDeltaSpeed : max_speed;
        mIsRotatingUp = true;
    }
    else if ( direction == CameraMovement::RIGHT )
    {
        if ( mSpeedForwardOrUp > 0 )
            mSpeedForwardOrUp = 0;
        mSpeedForwardOrUp = mSpeedForwardOrUp > -max_speed ? mSpeedForwardOrUp - mDeltaSpeed : -max_speed;
        mIsRotatingUp = true;
    }

    // Vertical speed (pitch) — guarded by mIsRotatingRight in OnUpdateThirdPerson
    if ( direction == CameraMovement::UP )
    {
        if ( mSpeedRight < 0 )
            mSpeedRight = 0;
        mSpeedRight = mSpeedRight < max_speed ? mSpeedRight + mDeltaSpeed : max_speed;
        mIsRotatingRight = true;
    }
    else if ( direction == CameraMovement::DOWN )
    {
        if ( mSpeedRight > 0 )
            mSpeedRight = 0;
        mSpeedRight = mSpeedRight > -max_speed ? mSpeedRight - mDeltaSpeed : -max_speed;
        mIsRotatingRight = true;
    }

    // Clamp
    if ( std::fabs( mSpeedForwardOrUp ) > max_speed )
        mSpeedForwardOrUp = Sign( mSpeedForwardOrUp ) * max_speed;

    if ( std::fabs( mSpeedRight ) > max_speed )
        mSpeedRight = Sign( mSpeedRight ) * max_speed;
}

void Camera::UpdateOrbit()
{
    // Spherical → Cartesian: position on the orbit sphere around mCenter
    mPosition.x = mCenter.x + mRadius * std::cos( mPitch ) * std::sin( mYaw );
    mPosition.y = mCenter.y - mRadius * std::sin( mPitch );
    mPosition.z = mCenter.z + mRadius * std::cos( mPitch ) * std::cos( mYaw );

    // Basis vectors
    mForward = normalize( mCenter - mPosition );
    mRight   = normalize( cross( mForward, mWorldUp ) );
    mUp      = normalize( cross( mRight, mForward ) );
}

void Camera::OnUpdateThirdPerson( float dt )
{
    // Inertia (driven by ProcessThirdPersonRotation)
    if ( !mIsRotatingUp )
        mSpeedForwardOrUp = std::abs( mSpeedForwardOrUp ) < 0.01f ? 0.0f : mSpeedForwardOrUp - Sign( mSpeedForwardOrUp ) * mDeltaSpeed * dt;

    if ( !mIsRotatingRight )
        mSpeedRight = std::abs( mSpeedRight ) < 0.01f ? 0.0f : mSpeedRight - Sign( mSpeedRight ) * mDeltaSpeed * dt;

    mIsRotatingUp    = false;
    mIsRotatingRight = false;

    mYaw   += mSpeedForwardOrUp * dt;
    mPitch += mSpeedRight * dt;
    mPitch  = std::clamp( mPitch, -camera::PI / 2.0f + 0.05f, camera::PI / 2.0f - 0.05f );

    UpdateOrbit();
}


}