
#include "engine/renderer/camera.hpp"

namespace bubble
{
Camera::Camera( const glm::vec3& position,
                float yaw,
                float pitch,
                float fov,
                const glm::vec3& up )
    : mPosition( position ),
    mWorldUp( up ),
    mYaw( yaw ),
    mPitch( pitch ),
    mFov( fov )
{
    UpdateCameraVectors();
}

glm::mat4 Camera::GetLookatMat() const
{
    return glm::lookAt( mPosition, mPosition + mFront, mUp );
}

glm::mat4 Camera::GetPprojectionMat( int window_width, int window_height ) const
{
    float aspect = (float)window_width / window_height;
    return glm::perspective<float>( mFov, aspect, mNear, mFar );
}

void Camera::UpdateCameraVectors()
{
    glm::vec3 front;

    front.x = std::cos( mYaw ) * std::cos( mPitch );
    front.y = std::sin( mPitch );
    front.z = std::sin( mYaw ) * std::cos( mPitch );

    mFront = glm::normalize( front );
    mRight = glm::normalize( glm::cross( mFront, mWorldUp ) );
    mUp = glm::normalize( glm::cross( mRight, mFront ) );
}
}