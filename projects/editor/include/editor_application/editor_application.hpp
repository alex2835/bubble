#pragma once
#include "engine/engine.hpp"
#include "engine/window/window.hpp"
#include "engine/project/project.hpp"
#include "engine/physics/physics_engine.hpp"
#include "editor_user_interface/editor_user_interface.hpp"
#include "editor_resources_hot_reloader/resources_hot_reloader.hpp"
#include "selection.hpp"
#include "ui_globals.hpp"
#include "history.hpp"

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
    Selection mSelection;
    History mHistory;
    Ref<ProjectTreeNode> mClipboard;
    bool mClipboardIsCut = false;
    
    // Viewport
    Framebuffer mSceneViewport;
    // Entity picking (Handles scene object picking in viewport)
    Framebuffer mEntityIdViewport;

    /// Game editing
    Project mProject;
    ProjectResourcesHotReloader mProjectResourcesHotReloader;
    // Game running
    Scene mSceneSave;
    Engine mEngine;

    EditorUserInterface mEditorUserInterface;

    // Editor state
    UIGlobals mUIGlobals;
};

}