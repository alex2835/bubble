#pragma once
#include "engine/utils/imexp.hpp"
#include "engine/renderer/camera.hpp"

namespace bubble
{
struct BUBBLE_ENGINE_EXPORT ThirdPersonCamera : public Camera
{
    vec3 mCenter;
    f32 mRadius = 20.0f;

    f32 mLastMouseX = 0.5f;
    f32 mLastMouseY = 0.5f;

    bool mIsRotatingX = false;
    bool mIsRotatingY = false;

    ThirdPersonCamera( f32 yaw = camera::YAW, 
                       f32 pitch = camera::PITCH );

    // Directions: UP, DOWN, LEFT, RIGHT
    void ProcessRotation( CameraMovement direction );
    void ProcessMouseMovement( f32 xMousePos, f32 yMousePos );
    void ProcessMouseMovementOffset( f32 xoffset, f32 yoffset );
    void ProcessMouseScroll( f32 yoffset );
    void OnUpdate( DeltaTime dt );
};

}