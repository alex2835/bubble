#pragma once
#include "engine/renderer/camera_free.hpp"

namespace bubble
{
struct SceneCamera : public FreeCamera
{
    SceneCamera( const WindowInput& input,
                 const vec3& pos = vec3( 0 ) )
        : mInput( input )
    {
        mPosition = pos;
        mMouseSensitivity = 0.001f;
    }

    void OnUpdate( DeltaTime dt )
    {
        if ( !mIsActive )
            return;

        mMaxSpeed = mInput.KeyMods().SHIFT ? mBoostSpeed : mDefaultSpeed;
        mDeltaSpeed = mMaxSpeed / 2;

        if ( mInput.IsKeyPressed( KeyboardKey::W ) )
            ProcessMovement( CameraMovement::FORWARD );

        if ( mInput.IsKeyPressed( KeyboardKey::S ) )
            ProcessMovement( CameraMovement::BACKWARD );

        if ( mInput.IsKeyPressed( KeyboardKey::A ) )
            ProcessMovement( CameraMovement::LEFT );

        if ( mInput.IsKeyPressed( KeyboardKey::D ) )
            ProcessMovement( CameraMovement::RIGHT );

        if ( mInput.IsKeyPressed( MouseKey::BUTTON_MIDDLE ) )
            ProcessMouseMovementOffset( mInput.MouseOffset().x, 
                                        mInput.MouseOffset().y );

        FreeCamera::OnUpdate( dt );
    }

public:
    f32  mBoostSpeed = 20.0f;
    f32  mDefaultSpeed = 10.0f;
    bool mIsActive = false;
private:
    const WindowInput& mInput;
};

}
