#pragma once
#include <glm/glm.hpp>
#include "engine/utils/imexp.hpp"
#include "engine/renderer/camera.hpp"

namespace bubble
{
struct BUBBLE_ENGINE_EXPORT ThirdPersonCamera : public Camera
{
    glm::vec3 mCenter;
    float mRadius = 20.0f;

    float mLastMouseX = 0.5f;
    float mLastMouseY = 0.5f;

    bool mIsRotatingX = false;
    bool mIsRotatingY = false;

    ThirdPersonCamera( float yaw = camera::YAW, 
                       float pitch = camera::PITCH );

    /*
        Directions: UP, DOWN, LEFT, RIGHT
    */
    void ProcessRotation( CameraMovement direction );
    void ProcessMouseMovement( float xMousePos, float yMousePos );
    void ProcessMouseMovementOffset( float xoffset, float yoffset );
    void ProcessMouseScroll( float yoffset );
    void OnUpdate( DeltaTime dt );
};

}