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
    bool mIsViewportHovered = false;

    // Menu
    bool mDrawBoundingBoxes = false;
    bool mDrawPhysicsShapes = false;
};

struct EditorResources
{
    // Visualization Camera, Lights billboards textures
    Ref<Texture2D> mSceneCameraTexture;
    Ref<Texture2D> mScenePointLightTexture;
    Ref<Texture2D> mSceneSpotLightTexture;
    Ref<Texture2D> mSceneDirLightTexture;
};

// Selection metadata
struct Selection
{
    // Selected node in project tree
    Ref<ProjectTreeNode> mProjectTreeNode;
    // All entities selected (Selected in project tree or on screen)
    set<Entity> mEntities;
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
    Ref<Shader> mEntityIdBillboardShader;

    /// Game editing
    Project mProject;
    ProjectResourcesHotReloader mProjectResourcesHotReloader;
    // Game running
    Scene mSceneSave;
    Engine mEngine;

    EditorUserInterface mEditorUserInterface;

    // Editor state
    UIGlobals mUIGlobals;

    // Editor resources
    EditorResources mEditorResources;

    // Billboards
    static constexpr auto cBillboardSize = vec2( 5.0f );
    static constexpr auto cBillboardTint = vec4( 1.0f );
    const Ref<Texture2D>& GetLightTexture( const LightType& lightType );
    void DrawEditorBillboards( Framebuffer& framebuffer, Scene& scene );
};

}