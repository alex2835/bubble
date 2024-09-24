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
        Runing
    };

    BubbleEditor();
    void OpenProject( const path& projectPath );
    void Run();
    void UpdateScripts();
    void DrawProjectScene();

public:
    EditorMode mEditorMode;
    Timer mTimer;
    Window mWindow;
    Framebuffer mSceneViewport;
    Framebuffer mObjectIdViewport;// Handles object picking in viewport
    Ref<Shader> mObjectIdShader;
    SceneCamera mSceneCamera;
    Entity mSelectedEntity;

    //Engine mEngine;
    Renderer mRenderer;
    Project mProject;
    ScriptingEngine mScriptingEngine;

    // UI global state
    bool mUINeedUpdateProjectWindow = false;

    // Editor user interface and resource hot reloader
    EditorUserInterface mEditorUserInterface;
    ResourcesHotReloader mResourcesHotReloader;
};

}