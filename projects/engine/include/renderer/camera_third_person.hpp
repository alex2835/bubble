#pragma once
#include <glm/glm.hpp>
#include "utils/imexp.hpp"
#include "renderer/camera.hpp"

namespace bubble
{
struct BUBBLE_ENGINE_EXPORT ThirdPersonCamera : public Camera
{
    glm::vec3 Center;
    float Radius = 20.0f;

    float LastMouseX = 0.5f;
    float LastMouseY = 0.5f;

    bool IsRotatingX = false;
    bool IsRotatingY = false;

    ThirdPersonCamera( float yaw = YAW, float pitch = PITCH );

    /*
        Directions: UP, DOWN, LEFT, RIGHT
    */
    void ProcessRotation( CameraMovement direction, DeltaTime dt );
    void ProcessMouseMovement( float xMousePos, float yMousePos );
    void ProcessMouseMovementShift( float xoffset, float yoffset );
    void ProcessMouseScroll( float yoffset );
    void Update( DeltaTime dt );
};

}