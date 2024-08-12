#pragma once
#include <imgui.h>
#undef APIENTRY
#include <imfilebrowser.h>
#undef min
#undef max

#include "editor_application/editor_state.hpp"
#include "engine/engine.hpp"

namespace bubble
{
class UserInterfaceWindowBase
{
public:
    UserInterfaceWindowBase( EditorState& editorState )
        : mWindow( editorState.mWindow ),
          mSceneViewport( editorState.mSceneViewport ),
          mObjectIdViewport( editorState.mObjectIdViewport ),
          mSceneCamera( editorState.mSceneCamera ),
          mEngine( editorState.mEngine ),
          mProject( editorState.mProject ),
          mSelectedEntity( editorState.mSelectedEntity ),
          mUINeedUpdateProjectWindow( editorState.mUINeedUpdateProjectWindow )
    {}

protected:
    bool mOpen = true;

    Window& mWindow;
    Framebuffer& mSceneViewport;
    Framebuffer& mObjectIdViewport;
    SceneCamera& mSceneCamera;
    Entity& mSelectedEntity;
    // Engine
    Engine& mEngine;
    // Project
    Project& mProject;
    // UI global state
    bool& mUINeedUpdateProjectWindow;
};

}
