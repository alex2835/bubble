#include "engine/pch/pch.hpp"
#include "utils/editor_settings.hpp"
#include "utils/ui_globals.hpp"
#include "utils/scene_camera.hpp"
#include "engine/serialization/types_serialization.hpp"
#include <nlohmann/json.hpp>
#include <fstream>

namespace bubble
{
path EditorSettings::FilePath()
{
    return filesystem::resolveNearExecutable( EDITOR_SETTINGS_FILE );
}

void EditorSettings::Save() const
{
    json settingsJson;

    json& interfaceJson = settingsJson["Interface"];
    interfaceJson["UIScale"] = mUIScale;
    interfaceJson["UIFontSize"] = mUIFontSize;
    interfaceJson["UIFontPath"] = mUIFontPath;

    json& cameraJson = settingsJson["Camera"];
    cameraJson["Position"] = mCameraPosition;
    cameraJson["Yaw"] = mCameraYaw;
    cameraJson["Pitch"] = mCameraPitch;
    cameraJson["Fov"] = mCameraFov;
    cameraJson["DefaultSpeed"] = mCameraDefaultSpeed;
    cameraJson["BoostSpeed"] = mCameraBoostSpeed;

    json& renderingJson = settingsJson["Rendering"];
    renderingJson["DrawBoundingBoxes"] = mDrawBoundingBoxes;
    renderingJson["DrawPhysicsShapes"] = mDrawPhysicsShapes;

    const auto filePath = FilePath();
    std::ofstream settingsFile( filePath );
    if ( not settingsFile.is_open() )
    {
        LogError( "Failed to save editor settings: {}", filePath.string() );
        return;
    }
    settingsFile << settingsJson.dump( 1 );
}

void EditorSettings::Load()
{
    const auto filePath = FilePath();
    if ( not filesystem::exists( filePath ) )
        return;

    try
    {
        std::ifstream settingsFile( filePath );
        json settingsJson = json::parse( settingsFile );

        // Every field is optional, a settings file written by an older build
        // must still load and just keep the defaults for whatever it is missing.
        if ( auto interfaceJson = settingsJson.find( "Interface" ); interfaceJson != settingsJson.end() )
        {
            mUIScale = interfaceJson->value( "UIScale", mUIScale );
            mUIFontSize = interfaceJson->value( "UIFontSize", mUIFontSize );
            mUIFontPath = interfaceJson->value( "UIFontPath", mUIFontPath );
        }

        if ( auto cameraJson = settingsJson.find( "Camera" ); cameraJson != settingsJson.end() )
        {
            if ( auto position = cameraJson->find( "Position" ); position != cameraJson->end() )
                from_json( *position, mCameraPosition );
            mCameraYaw = cameraJson->value( "Yaw", mCameraYaw );
            mCameraPitch = cameraJson->value( "Pitch", mCameraPitch );
            mCameraFov = cameraJson->value( "Fov", mCameraFov );
            mCameraDefaultSpeed = cameraJson->value( "DefaultSpeed", mCameraDefaultSpeed );
            mCameraBoostSpeed = cameraJson->value( "BoostSpeed", mCameraBoostSpeed );
        }

        if ( auto renderingJson = settingsJson.find( "Rendering" ); renderingJson != settingsJson.end() )
        {
            mDrawBoundingBoxes = renderingJson->value( "DrawBoundingBoxes", mDrawBoundingBoxes );
            mDrawPhysicsShapes = renderingJson->value( "DrawPhysicsShapes", mDrawPhysicsShapes );
        }
    }
    catch ( const std::exception& e )
    {
        // A corrupt settings file is not worth failing a startup over.
        LogError( "Failed to read editor settings: {}, using defaults", e.what() );
        *this = EditorSettings();
    }
}

void EditorSettings::Apply( Window& window, SceneCamera& camera, UIGlobals& uiGlobals ) const
{
    window.SetUIFont( mUIFontPath );
    window.SetUIFontSize( mUIFontSize );
    window.SetUIScale( mUIScale );

    camera.mPosition = mCameraPosition;
    camera.mYaw = mCameraYaw;
    camera.mPitch = mCameraPitch;
    camera.mFov = mCameraFov;
    camera.mDefaultSpeed = mCameraDefaultSpeed;
    camera.mBoostSpeed = mCameraBoostSpeed;
    // Yaw and pitch are the stored form, the basis vectors are derived from them.
    camera.EulerAnglesToVectors();

    uiGlobals.mDrawBoundingBoxes = mDrawBoundingBoxes;
    uiGlobals.mDrawPhysicsShapes = mDrawPhysicsShapes;
}

void EditorSettings::Capture( const Window& window, const SceneCamera& camera, const UIGlobals& uiGlobals )
{
    mUIScale = window.GetUIScale();
    mUIFontSize = window.GetUIFontSize();
    mUIFontPath = window.GetUIFont();

    mCameraPosition = camera.mPosition;
    mCameraYaw = camera.mYaw;
    mCameraPitch = camera.mPitch;
    mCameraFov = camera.mFov;
    mCameraDefaultSpeed = camera.mDefaultSpeed;
    mCameraBoostSpeed = camera.mBoostSpeed;

    mDrawBoundingBoxes = uiGlobals.mDrawBoundingBoxes;
    mDrawPhysicsShapes = uiGlobals.mDrawPhysicsShapes;
}

}
