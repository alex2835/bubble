#pragma once
#include "engine/engine.hpp"
#include "engine/scene/scene.hpp"
#include "engine/project/project_tree.hpp"
#include "utils/scene_camera.hpp"

namespace bubble
{
class BubbleEditor;
class Window;
class Selection;
class History;
struct UIGlobals;
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
    
    Project& mProject;
    Selection& mSelection;
    History& mHistory;

    // UI global state
    UIGlobals& mUIGlobals;
};

}
