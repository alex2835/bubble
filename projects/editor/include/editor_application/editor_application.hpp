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

// UI global state (Common variables for all interface windows and editor)
struct UIGlobals
{
    bool mNeedUpdateProjectWindow = false;
    bool mViewportHovered = false;

    // Menu
    bool mDrawBoundingBoxes = false;
    bool mDrawPhysicsShapes = false;
};

// Selection metadata
struct Selection
{
    // Selected node in project tree
    Ref<ProjectTreeNode> mPrejectTreeNode;
    // All entities in this sub tree
    vector<Entity> mEntities;
    // Used when selected more then one entity
    TransformComponent mGroupTransform;
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
    void DrawEntityIds();
public:
    Timer mTimer;
    Window mWindow;

    /// Editor
    EditorMode mEditorMode;
    SceneCamera mSceneCamera;
    Selection mSelection;
    
    // Viewport
    Framebuffer mSceneViewport;
    // Entity picking (Handles scene object picking in viewport)
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