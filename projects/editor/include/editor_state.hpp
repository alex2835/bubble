#pragma once 
#include "engine/engine.hpp"

namespace bubble
{
struct EditorState
{
    Timer mTimer;
    Window mWindow;
    Framebuffer mSceneViewport;
    Framebuffer mObjectIdViewport;
    SceneCamera mSceneCamera;

    Project mProject;
    Entity mSelectedEntity;
};

struct EditorStateRef
{
    EditorStateRef( EditorState& editorState )
        : mWindow( editorState.mWindow ),
          mSceneViewport( editorState.mSceneViewport ),
          mObjectIdViewport( editorState.mObjectIdViewport ),
          mSceneCamera( editorState.mSceneCamera ),
          mProject( editorState.mProject ),
          mSelectedEntity( editorState.mSelectedEntity )
    {}

    Window& mWindow;
    Framebuffer& mSceneViewport;
    Framebuffer& mObjectIdViewport;
    SceneCamera& mSceneCamera;

    Project& mProject;
    Entity& mSelectedEntity;
};
}
