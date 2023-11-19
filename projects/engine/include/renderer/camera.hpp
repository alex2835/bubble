#pragma once
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "utils/timer.hpp"
#include "utils/imexp.hpp"

namespace bubble
{
enum class CameraMovement
{
    FORWARD,
    BACKWARD,
    LEFT,
    RIGHT,
    UP,
    DOWN,
    NO_DIRECTION
};

namespace
{
const float PI = 3.14159265359f;
// Default camera values
const static float YAW = -PI / 2;
const static float PITCH = 0;
const static float FOV = PI / 4;
const static float DELTA_FOV = 0.05f;

const static float MAX_SPEED = 10.0f;
const static float DELTA_SPEED = 5.0f;
const static float SENSITIVTY = 4.25f;

template <typename T>
inline int sgn( T num )
{
    return ( num > T( 0 ) ) - ( num < T( 0 ) );
}
}


struct BUBBLE_ENGINE_EXPORT Camera
{
    // Camera Attributes
    glm::vec3 Position = glm::vec3( 0.0f, 0.0f, 0.0f );
    glm::vec3 Front = glm::vec3( 0.0f, 0.0f, -1.0f );
    glm::vec3 Up;
    glm::vec3 Right;
    glm::vec3 WorldUp = glm::vec3( 0.0f, 1.0f, 0.0f );

    float Near = 0.1f;
    float Far = 5000.0f;

    // Mouse
    float MouseSensitivity = SENSITIVTY;

    // Euler Angles
    float Yaw = YAW;
    float Pitch = PITCH;

    float Fov = FOV;
    float DeltaFov = DELTA_FOV;

    // Speed
    float MaxSpeed = MAX_SPEED;
    float DeltaSpeed = DELTA_SPEED;
    float SpeedX = 0;
    float SpeedY = 0;

public:
    Camera( const glm::vec3& position = glm::vec3( 0.0f, 0.0f, 0.0f ),
            float yaw = YAW,
            float pitch = PITCH,
            float fov = FOV,
            const glm::vec3& up = glm::vec3( 0.0f, 1.0f, 0.0f )
    );

    glm::mat4 GetLookatMat() const;
    glm::mat4 GetPprojectionMat( int window_width, int window_height ) const;

    void UpdateCameraVectors();
};
}