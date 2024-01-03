
#include "engine/renderer/camera_third_person.hpp"

namespace bubble
{

ThirdPersonCamera::ThirdPersonCamera( float yaw, float pitch )
    : mCenter( glm::vec3( 0, 0, 0 ) ),
    Camera( glm::vec3(), yaw, pitch )
{
}

void ThirdPersonCamera::ProcessRotation( CameraMovement direction )
{
    float max_speed = mMaxSpeed * mDeltaSpeed;

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


void ThirdPersonCamera::ProcessMouseMovement( float MousePosX, float MousePosY )
{
    float xoffset = ( mLastMouseX - MousePosX ) * mMouseSensitivity;
    float yoffset = ( mLastMouseY - MousePosY ) * mMouseSensitivity;

    mLastMouseX = MousePosX;
    mLastMouseY = MousePosY;

    mYaw -= xoffset;
    mPitch -= yoffset;

    if( mPitch > camera::PI / 2.0f - 0.1f )
        mPitch = camera::PI / 2.0f - 0.1f;

    if( mPitch < -camera::PI / 2.0f + 0.1f )
        mPitch = -camera::PI / 2.0f + 0.1f;
}


void ThirdPersonCamera::ProcessMouseMovementOffset( float xoffset, float yoffset )
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


void ThirdPersonCamera::ProcessMouseScroll( float yoffset )
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

    mYaw += mSpeedX * dt.GetSeconds();
    mPitch += mSpeedY * dt.GetSeconds();

    // Transformation matrix
    glm::mat4 transform = glm::mat4( 1.0f );
    transform = glm::rotate( transform, mYaw, glm::vec3( 0, 1, 0 ) );
    transform = glm::rotate( transform, mPitch, glm::vec3( 1, 0, 0 ) );
    transform = glm::translate( transform, mCenter );

    mPosition = transform * glm::vec4( 0, 0, mRadius, 0 );

    // Basis
    mFront = glm::normalize( mCenter - mPosition );
    mRight = glm::normalize( glm::cross( mFront, mWorldUp ) );
    mUp = glm::normalize( glm::cross( mRight, mFront ) );
}

}