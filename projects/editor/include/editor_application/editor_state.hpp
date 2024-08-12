#pragma once 
#include "engine/engine.hpp"
#include "utils/scene_camera.hpp"

namespace bubble
{
struct EditorState
{
    // Editor
    Timer mTimer;
    Window mWindow;
    Framebuffer mSceneViewport;
    Framebuffer mObjectIdViewport;
    SceneCamera mSceneCamera;
    Entity mSelectedEntity;
    // Engine 
    Engine mEngine;
    // Project
    Project mProject;
    // UI global state
    bool mUINeedUpdateProjectWindow = false;
};

}
