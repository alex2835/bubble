
#include "renderer/camera.hpp"

namespace bubble
{
Camera::Camera( const glm::vec3& position,
                float yaw,
                float pitch,
                float fov,
                const glm::vec3& up )
    : Position( position ),
    WorldUp( up ),
    Yaw( yaw ),
    Pitch( pitch ),
    Fov( fov )
{
    UpdateCameraVectors();
}

glm::mat4 Camera::GetLookatMat() const
{
    return glm::lookAt( Position, Position + Front, Up );
}

glm::mat4 Camera::GetPprojectionMat( int window_width, int window_height ) const
{
    float aspect = (float)window_width / window_height;
    return glm::perspective<float>( Fov, aspect, Near, Far );
}

void Camera::UpdateCameraVectors()
{
    glm::vec3 front;

    front.x = cosf( Yaw ) * cosf( Pitch );
    front.y = sinf( Pitch );
    front.z = sinf( Yaw ) * cosf( Pitch );

    Front = glm::normalize( front );
    Right = glm::normalize( glm::cross( Front, WorldUp ) );
    Up = glm::normalize( glm::cross( Right, Front ) );
}
}