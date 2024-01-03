#pragma once
#include "engine/utils/imexp.hpp"
#include "engine/renderer/camera.hpp"

namespace bubble
{
struct BUBBLE_ENGINE_EXPORT FreeCamera : public Camera
{
    float mLastMouseX = 0.5f;
    float mLastMouseY = 0.5f;

    bool mIsMovingX = false;
    bool mIsMovingY = false;

    FreeCamera( const glm::vec3& position = glm::vec3( 0.0f, 0.0f, 0.0f ),
                float yaw = camera::YAW,
                float pitch = camera::PITCH,
                float fov = camera::FOV,
                const glm::vec3& up = glm::vec3( 0.0f, 1.0f, 0.0f )
    );

    void ProcessMovement( CameraMovement direction );
    void ProcessMouseMovement( float xMousePos, float yMousePos );
    void ProcessMouseMovementOffset( float xoffset, float yoffset );
    void ProcessMouseScroll( float yoffset );

    void OnUpdate( DeltaTime dt );
};
}
