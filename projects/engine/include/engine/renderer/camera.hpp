#pragma once
#include "glm/gtc/matrix_transform.hpp"
#include "engine/utils/timer.hpp"
#include "engine/utils/math.hpp"
#include "engine/window/event.hpp"

namespace bubble
{
enum class CameraMovement
{
    FORWARD,
    BACKWARD,
    LEFT,
    RIGHT,
    UP,
    DOWN
};

// Default camera values
namespace camera
{
constexpr f32 PI = 3.14159265359f;
constexpr f32 YAW = -PI / 2;
constexpr f32 PITCH = 0;
constexpr f32 FOV = PI / 4;
constexpr f32 DELTA_FOV = 0.05f;

constexpr f32 MAX_SPEED = 10.0f;
constexpr f32 DELTA_SPEED = 5.0f;
constexpr f32 SENSITIVTY = 4.25f;
}


struct Camera
{
    // Camera Attributes
    vec3 mPosition = vec3( 0.0f, 0.0f, 0.0f );
    vec3 mForward  = vec3( 0.0f, 0.0f, -1.0f );
    vec3 mUp       = vec3( 0 );
    vec3 mRight    = vec3( 0 );
    vec3 mWorldUp  = vec3( 0.0f, 1.0f, 0.0f );

    f32 mNear = 0.1f;
    f32 mFar = 5000.0f;

    // Mouse
    f32 mMouseSensitivity = camera::SENSITIVTY;
    f32 mLastMouseX = 0.5f;
    f32 mLastMouseY = 0.5f;

    // Fov
    f32 mFov = camera::FOV;
    f32 mDeltaFov = camera::DELTA_FOV;


    // Free camera
    // Euler Angles
    f32 mYaw = camera::YAW;
    f32 mPitch = camera::PITCH;

    // Speed
    f32 mMaxSpeed = camera::MAX_SPEED;
    f32 mDeltaSpeed = camera::DELTA_SPEED;
    f32 mSpeedForwardOrUp = 0;
    f32 mSpeedRight = 0;
    bool mIsMovingForward = false;
    bool mIsMovingRight = false;

    // Third person camera
    vec3 mCenter;
    f32 mRadius = 20.0f;
    bool mIsRotatingRight = false;
    bool mIsRotatingUp = false;


public:
    Camera( vec3 position = vec3( 0.0f, 0.0f, 0.0f ),
            f32 yaw = camera::YAW,
            f32 pitch = camera::PITCH,
            f32 fov = camera::FOV,
            vec3 up = vec3( 0.0f, 1.0f, 0.0f )
    );

    mat4 GetLookatMat() const;
    mat4 GetPprojectionMat( i32 window_width, i32 window_height ) const;

    // Free camera 
    void ProcessMovement( CameraMovement direction );
    void ProcessMouseMovement( f32 xMousePos, f32 yMousePos );
    void ProcessMouseMovementOffset( f32 xOffset, f32 yOffset );
    void ProcessMouseScroll( f32 offset );
    void EulerAnglesToVectors();
    void OnUpdateFreeCamera( DeltaTime dt );

    // Third person camera
    void ProcessRotation( CameraMovement direction );
    void OnUpdateThirdPerson( DeltaTime dt );

};
}