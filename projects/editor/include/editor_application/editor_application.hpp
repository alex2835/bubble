#pragma once
#include "engine/engine.hpp"
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

    void StartGame();
    void StartEditing();

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

    ScriptingEngine mScriptingEngine;
    Loader mLoader;
    // Game to edit
    Project mProject;
    // Game runner 
    Engine mEngine;

    // UI global state
    bool mUINeedUpdateProjectWindow = false;

    // Editor's user interface and resource hot reloader
    EditorUserInterface mEditorUserInterface;
    ResourcesHotReloader mResourcesHotReloader;
};

}