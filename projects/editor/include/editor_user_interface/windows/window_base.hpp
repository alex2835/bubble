#pragma once
#include "engine/engine.hpp"
#include "engine/scene/scene.hpp"
#include "engine/project/project_tree.hpp"
#include "utils/scene_camera.hpp"

namespace bubble
{
class BubbleEditor;
class Window;
struct UIGlobals;
struct Selection;
enum class EditorMode;

// View of an editor
class UserInterfaceWindowBase
{
public:
    UserInterfaceWindowBase( BubbleEditor& editor );
    // void SetSeleciton( const Ref<ProjectTreeNode>& node );

protected:
    bool mOpen = true;

    Window& mWindow;
    EditorMode& mEditorMode;
    Framebuffer& mSceneViewport;
    Framebuffer& mEntityIdViewport;
    SceneCamera& mSceneCamera;

    Selection& mSelection;
    
    Project& mProject;

    // UI global state
    UIGlobals& mUIGlobals;
};

}
