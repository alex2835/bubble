#pragma once
#include "engine/renderer/camera_free.hpp"

namespace bubble
{
struct SceneCamera : public FreeCamera
{
    //float  mBoostSpeed = 5.0f;

    SceneCamera()
    {
        mMouseSensitivity = 0.001f;
    }

    void OnEvent( const Event& event )
    {
        switch ( event.mType )
        {
        case bubble::EventType::KeyboardKey:
        {
            switch ( event.mKeyboard.Action )
            {
            case KeyAction::Press:
            case KeyAction::Repeat:
            {
                switch ( event.mKeyboard.Key )
                {
                case KeyboardKey::W:
                    ProcessMovement( CameraMovement::FORWARD );
                    break;
                case KeyboardKey::S:
                    ProcessMovement( CameraMovement::BACKWARD );
                    break;
                case KeyboardKey::A:
                    ProcessMovement( CameraMovement::LEFT );
                    break;
                case KeyboardKey::D:
                    ProcessMovement( CameraMovement::RIGHT );
                    break;
                }
            }
            }
        }break;

        case bubble::EventType::MouseKey:
            break;
        case bubble::EventType::MouseMove:
            if ( event.mMouseInput->IsKeyPressed( MouseKey::BUTTON_MIDDLE ) )
                ProcessMouseMovementOffset( event.mMouse.Offset.x, event.mMouse.Offset.y );
            break;
        case bubble::EventType::MouseZoom:
            break;
        default:
            break;
        }
    }

    void OnUpdate( DeltaTime dt )
    {
        FreeCamera::OnUpdate( dt );
    }

};

}
