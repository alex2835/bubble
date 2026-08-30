#pragma once
#include "engine/types/number.hpp"
#include "engine/types/glm.hpp"
#include "engine/types/string.hpp"
#include "engine/utils/filesystem.hpp"
#include "engine/renderer/camera.hpp"
#include "engine/window/window.hpp"

namespace bubble
{
struct UIGlobals;
struct SceneCamera;

// Stored next to the executable so it survives rebuilds and does not depend on
// the working directory the editor was launched from.
constexpr string_view EDITOR_SETTINGS_FILE = "editor_settings.json"sv;

// Editor preferences that outlive a session. Deliberately not part of a project,
// these follow the machine and the user, not the thing being edited.
struct EditorSettings
{
    /// Window geometry
    // False until a session has been captured or a file has been read, the
    // first run has to let the OS place the window instead of restoring junk.
    bool mHasWindowGeometry = false;
    ivec2 mWindowPosition = ivec2( 0 );
    uvec2 mWindowSize = uvec2( 1920, 1080 );
    bool mWindowMaximized = false;

    /// Interface
    f32 mUIScale = 1.0f;
    f32 mUIFontSize = DEFAULT_UI_FONT_SIZE;
    string mUIFontPath = string( DEFAULT_UI_FONT_PATH );

    /// Scene camera
    vec3 mCameraPosition = vec3( 0.0f, 0.0f, 100.0f );
    f32 mCameraYaw = camera::YAW;
    f32 mCameraPitch = camera::PITCH;
    f32 mCameraFov = camera::FOV;
    f32 mCameraDefaultSpeed = 50.0f;
    f32 mCameraBoostSpeed = 100.0f;

    /// Rendering helpers from the Options menu
    bool mDrawBoundingBoxes = false;
    bool mDrawPhysicsShapes = false;

    static path FilePath();

    // A missing or unreadable file leaves the defaults in place, settings are a
    // convenience and must never stop the editor from starting.
    void Load();
    void Save() const;

    // Settings -> live editor state.
    void Apply( Window& window, SceneCamera& camera, UIGlobals& uiGlobals ) const;
    // Live editor state -> settings.
    void Capture( const Window& window, const SceneCamera& camera, const UIGlobals& uiGlobals );
};

}
