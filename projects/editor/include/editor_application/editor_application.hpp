#pragma once
#include "engine/engine.hpp"
#include "engine/window/window.hpp"
#include "engine/project/project.hpp"
#include "engine/physics/physics_engine.hpp"
#include "editor_user_interface/editor_user_interface.hpp"
#include "utils/resources_hot_reloader.hpp"
#include "utils/selection.hpp"
#include "utils/ui_globals.hpp"
#include "utils/history.hpp"
#include "utils/clipboard.hpp"
#include "utils/auto_backup.hpp"

namespace bubble
{
enum class EditorMode
{
    Editing,
    Running
};

// Game engine editor 
class BubbleEditor
{
public:
    BubbleEditor();
    void OpenProject( const path& projectPath );
    void Run();

private:
    void OnUpdate();
    void OnUpdateHotKeys();

public:
    Timer mTimer;
    Window mWindow;

    /// Editor
    EditorMode mEditorMode;
    SceneCamera mSceneCamera;
    Framebuffer mSceneViewport; // Viewport
    Framebuffer mEntityIdViewport; // Entity picking viewport (Handles scene object picking in viewport)

    /// Game editing
    Project mProject;
    Selection mSelection;
    History mHistory;
    Clipboard mClipboard;
    AutoBackup mAutoBackup;
    ProjectResourcesHotReloader mProjectResourcesHotReloader;

    // Game running
    Scene mSceneSave;
    Engine mEngine;

    EditorUserInterface mEditorUserInterface;

    // Editor state
    UIGlobals mUIGlobals;
};

}