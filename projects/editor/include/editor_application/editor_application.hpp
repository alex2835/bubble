#pragma once
#include "engine/engine.hpp"
#include "engine/window/window.hpp"
#include "engine/project/project.hpp"
#include "engine/physics/physics_engine.hpp"
#include "editor_user_interface/editor_user_interface.hpp"
#include "editor_resources_hot_reloader/resources_hot_reloader.hpp"

namespace bubble
{
enum class EditorMode
{
    Editing,
    Running
};

// UI global state (Common variables for all interface windows)
struct UIGlobals
{
    bool mNeedUpdateProjectWindow = false;
};


class BubbleEditor
{
public:
    BubbleEditor();
    void OpenProject( const path& projectPath );
    void Run();

private:
    void OnUpdate();
    void DrawEntityIds();
public:
    Timer mTimer;
    Window mWindow;

    /// Editor
    EditorMode mEditorMode;
    SceneCamera mSceneCamera;
    Entity mSelectedEntity;
    
    // Viewport
    Framebuffer mSceneViewport;
    // Entity picking (Handles object picking in viewport)
    Framebuffer mEntityIdViewport;
    Ref<Shader> mEntityIdShader;

    // Game editing
    Project mProject;
    // Game running
    Scene mSceneSave;
    Engine mEngine;


    ProjectResourcesHotReloader mProjectResourcesHotReloader;
    EditorUserInterface mEditorUserInterface;

    UIGlobals mUIGlobals;
};

}