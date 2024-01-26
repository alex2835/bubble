
#include "engine/renderer/camera.hpp"

namespace bubble
{
Camera::Camera( const vec3& position,
                f32 yaw,
                f32 pitch,
                f32 fov,
                const vec3& up )
    : mPosition( position ),
      mWorldUp( up ),
      mYaw( yaw ),
      mPitch( pitch ),
      mFov( fov )
{
    UpdateCameraVectors();
}

mat4 Camera::GetLookatMat() const
{
    return lookAt( mPosition, mPosition + mFront, mUp );
}

mat4 Camera::GetPprojectionMat( i32 window_width, i32 window_height ) const
{
    f32 aspect = (f32)window_width / window_height;
    return perspective<f32>( mFov, aspect, mNear, mFar );
}

void Camera::UpdateCameraVectors()
{
    vec3 front;

    front.x = std::cos( mYaw ) * std::cos( mPitch );
    front.y = std::sin( mPitch );
    front.z = std::sin( mYaw ) * std::cos( mPitch );

    mFront = normalize( front );
    mRight = normalize( cross( mFront, mWorldUp ) );
    mUp = normalize( cross( mRight, mFront ) );
}
}