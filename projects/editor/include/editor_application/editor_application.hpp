#pragma once
#include "engine/engine.hpp"
#include "engine/window/window.hpp"
#include "engine/project/project.hpp"
#include "engine/physics/physics_engine.hpp"
#include "editor_user_interface/editor_user_interface.hpp"
#include "editor_resources_hot_reloader/resources_hot_reloader.hpp"

namespace bubble
{
// UI global state (Common variables for all interface windows)
struct UIGlobals
{
    bool mNeedUpdateProjectWindow = false;
};


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

private:
    void OnUpdate();
    void DrawProjectScene();

    void DrawBBoxes();
    void DrawPhysics();

public:
    Timer mTimer;
    Window mWindow;

    /// Editor
    EditorMode mEditorMode;
    SceneCamera mSceneCamera;
    Entity mSelectedEntity;
    
    // Viewport
    Framebuffer mSceneViewport;
    // Entity picking
    Framebuffer mObjectIdViewport; // Handles object picking in viewport
    Ref<Shader> mObjectIdShader;

    // Game editing
    Project mProject;
    // Game running
    Scene mSceneSave;
    Engine mEngine;


    Ref<Shader> mWhiteShader;
    // Draw all bouding boxes as single mesh
    VertexBufferData mBBoxesVerts;
    vector<u32> mBBoxesIndices;
    Mesh mBBoxsMesh;

    // Draw all physics as single mesh
    VertexBufferData mPhysicsObjectsVerts;
    vector<u32> mPhysicsObjectsIndices;
    Mesh mPhysicsObjectsMesh;


    ProjectResourcesHotReloader mProjectResourcesHotReloader;
    EditorUserInterface mEditorUserInterface;

    UIGlobals mUIGlobals;
};

}