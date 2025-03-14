#pragma once
#include "engine/renderer/camera.hpp"

namespace bubble
{
struct SceneCamera : public Camera
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
        if ( mIsActive )
        {
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

            if ( mInput.IsKeyPressed( MouseKey::RIGHT ) )
                ProcessMouseMovementOffset( mInput.MouseOffset().x, 
                                            mInput.MouseOffset().y );
        }
        OnUpdateFreeCamera( dt );
    }

public:
    f32  mBoostSpeed = 20.0f;
    f32  mDefaultSpeed = 10.0f;
    bool mIsActive = false;
private:
    const WindowInput& mInput;
};

}
