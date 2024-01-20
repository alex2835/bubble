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

    FreeCamera( const vec3& position = vec3( 0.0f, 0.0f, 0.0f ),
                f32 yaw = camera::YAW,
                f32 pitch = camera::PITCH,
                f32 fov = camera::FOV,
                const vec3& up = vec3( 0.0f, 1.0f, 0.0f )
    );

    void ProcessMovement( CameraMovement direction );
    void ProcessMouseMovement( f32 xMousePos, f32 yMousePos );
    void ProcessMouseMovementOffset( f32 xoffset, f32 yoffset );
    void ProcessMouseScroll( f32 yoffset );

    void OnUpdate( DeltaTime dt );
};
}
