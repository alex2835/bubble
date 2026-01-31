
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

void Camera::ProcessMovement( DeltaTime dt, CameraMovement direction )
{
    f32 max_speed = mMaxSpeed * mDeltaSpeed;

    // Speed forward
    if ( direction == CameraMovement::FORWARD )
    {
        if ( mSpeedForwardOrUp < 0 )
            mSpeedForwardOrUp *= 0.5f;
        mSpeedForwardOrUp = mSpeedForwardOrUp + mDeltaSpeed * dt.Seconds();
        mIsMovingForward = true;
    }
    else if ( direction == CameraMovement::BACKWARD )
    {
        if ( mSpeedForwardOrUp > 0 )
            mSpeedForwardOrUp *= 0.5f;
        mSpeedForwardOrUp = mSpeedForwardOrUp - mDeltaSpeed * dt.Seconds();
        mIsMovingForward = true;
    }

    // Speed right
    if ( direction == CameraMovement::RIGHT )
    {
        if ( mSpeedRight < 0 )
            mSpeedRight *= 0.5f;
        mSpeedRight = mSpeedRight + mDeltaSpeed * dt.Seconds();
        mIsMovingRight = true;
    }
    else if ( direction == CameraMovement::LEFT )
    {
        if ( mSpeedRight > 0 )
            mSpeedRight *= 0.5f;
        mSpeedRight = mSpeedRight - mDeltaSpeed * dt.Seconds();
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

void Camera::OnUpdateFreeCamera( DeltaTime dt )
{
    // Inertia
    if ( !mIsMovingForward )
    {
        if ( std::abs( mSpeedForwardOrUp ) < 0.1f * mDeltaSpeed )
            mSpeedForwardOrUp = 0;
        else
            mSpeedForwardOrUp = mSpeedForwardOrUp - Sign( mSpeedForwardOrUp ) * mDeltaSpeed * dt.Seconds();
    }

    if ( !mIsMovingRight )
    {
        if ( std::abs( mSpeedRight ) < 0.1f * mDeltaSpeed )
            mSpeedRight = 0;
        else
            mSpeedRight = mSpeedRight - Sign( mSpeedRight ) * mDeltaSpeed * dt.Seconds();
    }

    mIsMovingForward = false;
    mIsMovingRight = false;

    mPosition += mForward * mSpeedForwardOrUp * dt.Seconds();
    mPosition += mRight * mSpeedRight * dt.Seconds();

    EulerAnglesToVectors();
}



// Third person camera 


void Camera::ProcessRotation( CameraMovement direction )
{
    f32 max_speed = mMaxSpeed * mDeltaSpeed;

    // Horizontal speed
    if ( direction == CameraMovement::LEFT )
    {   
        if ( mSpeedForwardOrUp < 0 )
            mSpeedForwardOrUp = 0;
        mSpeedForwardOrUp = mSpeedForwardOrUp < max_speed ? mSpeedForwardOrUp + mDeltaSpeed : max_speed;
        mIsRotatingRight = true;
    }
    else if ( direction == CameraMovement::RIGHT )
    {
        if ( mSpeedForwardOrUp > 0 )
            mSpeedForwardOrUp = 0;
        mSpeedForwardOrUp = mSpeedForwardOrUp > -max_speed ? mSpeedForwardOrUp - mDeltaSpeed : -max_speed;
        mIsRotatingRight = true;
    }

    // Vertical speed
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


void Camera::OnUpdateThirdPerson( DeltaTime dt )
{
    // Inertia
    if ( !mIsRotatingUp )
        mSpeedForwardOrUp = std::abs( mSpeedForwardOrUp ) < 0.01f ? mSpeedForwardOrUp = 0 : mSpeedForwardOrUp - Sign( mSpeedForwardOrUp ) * mDeltaSpeed;

    if ( !mIsRotatingRight )
        mSpeedRight = std::abs( mSpeedRight ) < 0.01f ? mSpeedRight = 0 : mSpeedRight - Sign( mSpeedRight ) * mDeltaSpeed;

    mIsRotatingUp = false;
    mIsRotatingRight = false;

    mYaw += mSpeedForwardOrUp * dt;
    mPitch += mSpeedRight * dt;

    // Transformation matrix
    mat4 transform = mat4( 1.0f );
    transform = rotate( transform, mYaw, vec3( 0, 1, 0 ) );
    transform = rotate( transform, mPitch, vec3( 1, 0, 0 ) );
    transform = translate( transform, mCenter );

    mPosition = transform * vec4( 0, 0, mRadius, 0 );

    // Basis
    mForward = normalize( mCenter - mPosition );
    mRight = normalize( cross( mForward, mWorldUp ) );
    mUp = normalize( cross( mRight, mForward ) );
}


}