#pragma once
#include "engine/utils/imexp.hpp"
#include "engine/renderer/camera.hpp"

namespace bubble
{
struct BUBBLE_ENGINE_EXPORT FreeCamera : public Camera
{
    f32 mLastMouseX = 0.5f;
    f32 mLastMouseY = 0.5f;

    bool mIsMovingX = false;
    bool mIsMovingY = false;

    using Camera::Camera;

    void ProcessMovement( CameraMovement direction );
    void ProcessMouseMovement( f32 xMousePos, f32 yMousePos );
    void ProcessMouseMovementOffset( f32 xOffset, f32 yOffset );
    void ProcessMouseScroll( f32 offset );

    void OnUpdate( DeltaTime dt );
};
}
