#pragma once
#include "engine/engine.hpp"
#include "engine/window/window.hpp"
#include "engine/project/project.hpp"
#include "engine/physics/physics_engine.hpp"
#include "editor_user_interface/editor_user_interface.hpp"
#include "editor_resources_hot_reloader/resources_hot_reloader.hpp"

namespace bubble
{
class BubbleEditor
{
public:
    enum class EditorMode
    {
        Editing,
        Running
    };

    BubbleEditor();
    void OpenProject( const path& projectPath );
    void Run();
    void DrawProjectScene();

public:
    Timer mTimer;
    Window mWindow;

    // Editor
    EditorMode mEditorMode;
    Framebuffer mSceneViewport;
    Framebuffer mObjectIdViewport; // Handles object picking in viewport
    Ref<Shader> mObjectIdShader;
    SceneCamera mSceneCamera;
    Entity mSelectedEntity;

    // Game to edit
    Project mProject;
    // Game runner
    Scene mSceneSave;
    Engine mEngine;

    // UI global state
    bool mUINeedUpdateProjectWindow = false;

    ProjectResourcesHotReloader mProjectResourcesHotReloader;
    EditorUserInterface mEditorUserInterface;
};

}