#pragma once
#include "engine/engine.hpp"
#include "engine/scene/scene.hpp"
#include "utils/scene_camera.hpp"

namespace bubble
{
class Window;
class BubbleEditor;
struct UIGlobals;

class UserInterfaceWindowBase
{
public:
    UserInterfaceWindowBase( BubbleEditor& editor );

protected:
    bool mOpen = true;

    Window& mWindow;
    Framebuffer& mSceneViewport;
    Framebuffer& mObjectIdViewport;
    SceneCamera& mSceneCamera;
    Entity& mSelectedEntity;
    
    Project& mProject;

    // UI global state
    UIGlobals& mUIGlobals;
};

}
