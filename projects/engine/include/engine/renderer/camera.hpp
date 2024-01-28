#pragma once
#include "glm/gtc/matrix_transform.hpp"
#include "engine/utils/timer.hpp"
#include "engine/utils/imexp.hpp"
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


struct BUBBLE_ENGINE_EXPORT Camera
{
    // Camera Attributes
    vec3 mPosition = vec3( 0.0f, 0.0f, 0.0f );
    vec3 mFront    = vec3( 0.0f, 0.0f, -1.0f );
    vec3 mUp       = vec3( 0 );
    vec3 mRight    = vec3( 0 );
    vec3 mWorldUp  = vec3( 0.0f, 1.0f, 0.0f );

    f32 mNear = 0.1f;
    f32 mFar = 5000.0f;

    // Mouse
    f32 mMouseSensitivity = camera::SENSITIVTY;

    // Euler Angles
    f32 mYaw = camera::YAW;
    f32 mPitch = camera::PITCH;
    f32 mFov = camera::FOV;
    f32 mDeltaFov = camera::DELTA_FOV;

    // Speed
    f32 mMaxSpeed = camera::MAX_SPEED;
    f32 mDeltaSpeed = camera::DELTA_SPEED;
    f32 mSpeedX = 0;
    f32 mSpeedY = 0;

public:
    Camera( const vec3& position = vec3( 0.0f, 0.0f, 0.0f ),
            f32 yaw = camera::YAW,
            f32 pitch = camera::PITCH,
            f32 fov = camera::FOV,
            const vec3& up = vec3( 0.0f, 1.0f, 0.0f )
    );

    mat4 GetLookatMat() const;
    mat4 GetPprojectionMat( i32 window_width, i32 window_height ) const;

    void UpdateCameraVectors();
};
}