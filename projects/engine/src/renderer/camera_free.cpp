
#include "renderer/camera_free.hpp"

namespace bubble
{
FreeCamera::FreeCamera( const glm::vec3& position,
                        float yaw,
                        float pitch,
                        float fov,
                        const glm::vec3& up )
    : Camera( position, yaw, pitch, fov, up )
{
}

void FreeCamera::ProcessMovement( CameraMovement direction, DeltaTime dt )
{
    float max_speed = MaxSpeed * DeltaSpeed;

    // Speed x
    if( direction == CameraMovement::FORWARD )
    {
        if( SpeedX < 0 )
            SpeedX = 0;
        SpeedX = SpeedX < max_speed ? SpeedX + DeltaSpeed : max_speed;
        IsMovingX = true;
    }
    else if( direction == CameraMovement::BACKWARD )
    {
        if( SpeedX > 0 )
            SpeedX = 0;
        SpeedX = SpeedX > -max_speed ? SpeedX - DeltaSpeed : -max_speed;
        IsMovingX = true;
    }

    // Speed y
    if( direction == CameraMovement::RIGHT )
    {
        if( SpeedY < 0 )
            SpeedY = 0;
        SpeedY = SpeedY < max_speed ? SpeedY + DeltaSpeed : max_speed;
        IsMovingY = true;
    }
    else if( direction == CameraMovement::LEFT )
    {
        if( SpeedY > 0 )
            SpeedY = 0;
        SpeedY = SpeedY > -max_speed ? SpeedY - DeltaSpeed : -max_speed;
        IsMovingY = true;
    }

    // Speed z
    // ...

    // Clamp
    if( fabs( SpeedX ) > max_speed )
        SpeedX = sgn( SpeedX ) * max_speed;

    if( fabs( SpeedY ) > max_speed )
        SpeedY = sgn( SpeedY ) * max_speed;
}


void FreeCamera::ProcessMouseMovement( float MousePosX, float MousePosY )
{
    float xoffset = ( LastMouseX - MousePosX ) * MouseSensitivity;
    float yoffset = ( LastMouseY - MousePosY ) * MouseSensitivity;

    LastMouseX = MousePosX;
    LastMouseY = MousePosY;

    Yaw -= xoffset;
    Pitch -= yoffset;

    if( Pitch > PI / 2.0f - 0.1f )
        Pitch = PI / 2.0f - 0.1f;

    if( Pitch < -PI / 2.0f + 0.1f )
        Pitch = -PI / 2.0f + 0.1f;
}

void FreeCamera::ProcessMouseMovementShift( float xoffset, float yoffset )
{
    xoffset *= MouseSensitivity;
    yoffset *= MouseSensitivity;

    Yaw += xoffset;
    Pitch += yoffset;

    if( Pitch > PI / 2.0f - 0.1f )
        Pitch = PI / 2.0f - 0.1f;

    if( Pitch < -PI / 2.0f + 0.1f )
        Pitch = -PI / 2.0f + 0.1f;
}


void FreeCamera::ProcessMouseScroll( float yoffset )
{
    if( Fov >= 0.1f && Fov <= PI / 2.0f )
        Fov += yoffset * DeltaFov;

    if( Fov < 0.1f )
        Fov = 0.1f;

    if( Fov > PI / 2.0f )
        Fov = PI / 2.0f;
}

void FreeCamera::Update( DeltaTime dt )
{
    // Inertia
    if( !IsMovingX )
        SpeedX = fabs( SpeedX ) < 0.01f ? SpeedX = 0 : SpeedX - sgn( SpeedX ) * DeltaSpeed;

    if( !IsMovingY )
        SpeedY = fabs( SpeedY ) < 0.01f ? SpeedY = 0 : SpeedY - sgn( SpeedY ) * DeltaSpeed;

    IsMovingX = false;
    IsMovingY = false;

    Position += Front * SpeedX * dt.GetSeconds();
    Position -= Right * SpeedY * dt.GetSeconds();

    UpdateCameraVectors();
}

}