#pragma once
#include "glm/glm.hpp"
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
constexpr float PI = 3.14159265359f;
constexpr float YAW = -PI / 2;
constexpr float PITCH = 0;
constexpr float FOV = PI / 4;
constexpr float DELTA_FOV = 0.05f;

constexpr float MAX_SPEED = 10.0f;
constexpr float DELTA_SPEED = 5.0f;
constexpr float SENSITIVTY = 4.25f;
}


struct BUBBLE_ENGINE_EXPORT Camera
{
    // Camera Attributes
    glm::vec3 mPosition = glm::vec3( 0.0f, 0.0f, 0.0f );
    glm::vec3 mFront    = glm::vec3( 0.0f, 0.0f, -1.0f );
    glm::vec3 mUp       = glm::vec3( 0 );
    glm::vec3 mRight    = glm::vec3( 0 );
    glm::vec3 mWorldUp  = glm::vec3( 0.0f, 1.0f, 0.0f );

    float mNear = 0.1f;
    float mFar = 5000.0f;

    // Mouse
    float mMouseSensitivity = camera::SENSITIVTY;

    // Euler Angles
    float mYaw   = camera::YAW;
    float mPitch = camera::PITCH;
    float mFov      = camera::FOV;
    float mDeltaFov = camera::DELTA_FOV;

    // Speed
    float mMaxSpeed   = camera::MAX_SPEED;
    float mDeltaSpeed = camera::DELTA_SPEED;
    float mSpeedX = 0;
    float mSpeedY = 0;

public:
    Camera( const glm::vec3& position = glm::vec3( 0.0f, 0.0f, 0.0f ),
            float yaw   = camera::YAW,
            float pitch = camera::PITCH,
            float fov   = camera::FOV,
            const glm::vec3& up = glm::vec3( 0.0f, 1.0f, 0.0f )
    );

    glm::mat4 GetLookatMat() const;
    glm::mat4 GetPprojectionMat( int window_width, int window_height ) const;

    void UpdateCameraVectors();
};
}