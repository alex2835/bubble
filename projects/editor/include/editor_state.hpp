#pragma once 
#include "engine/engine.hpp"

namespace bubble
{
struct EditorState
{
    bool IsKeyCliked( KeyboardKey key ) const { return mWindow.GetKeyboardInput().IsKeyCliked( key ); };
    i32 IsKeyPressed( KeyboardKey key ) const { return mWindow.GetKeyboardInput().IsKeyCliked( key ); };
    bool IsKeyCliked( MouseKey key ) const { return mWindow.GetMouseInput().IsKeyCliked( key ); };
    i32 IsKeyPressed( MouseKey key ) const { return mWindow.GetMouseInput().IsKeyPressed( key ); };

public:
    // Editor
    Timer mTimer;
    Window mWindow;
    Framebuffer mSceneViewport;
    Framebuffer mObjectIdViewport;
    SceneCamera mSceneCamera;
    // Engine 
    Engine mEngine;
    // Project
    Project mProject;
    Entity mSelectedEntity;
    // UI global state
    bool mUINeedUpdateProjectInterface = false;
};

struct EditorStateRef
{
    EditorStateRef( EditorState& editorState )
        : mWindow( editorState.mWindow ),
          mSceneViewport( editorState.mSceneViewport ),
          mObjectIdViewport( editorState.mObjectIdViewport ),
          mSceneCamera( editorState.mSceneCamera ),
          mEngine( editorState.mEngine ),
          mProject( editorState.mProject ),
          mSelectedEntity( editorState.mSelectedEntity ),
          mUINeedUpdateProjectInterface( editorState.mUINeedUpdateProjectInterface )
    {}
    bool IsKeyCliked( KeyboardKey key ) const { return mWindow.GetKeyboardInput().IsKeyCliked( key ); };
    i32 IsKeyPressed( KeyboardKey key ) const { return mWindow.GetKeyboardInput().IsKeyCliked( key ); };
    bool IsKeyCliked( MouseKey key ) const { return mWindow.GetMouseInput().IsKeyCliked( key ); };
    i32 IsKeyPressed( MouseKey key ) const { return mWindow.GetMouseInput().IsKeyPressed( key ); };

public:
    // Editor
    Window& mWindow;
    Framebuffer& mSceneViewport;
    Framebuffer& mObjectIdViewport;
    SceneCamera& mSceneCamera;
    // Engine
    Engine& mEngine;
    // Project
    Project& mProject;
    Entity& mSelectedEntity;
    // UI global state
    bool& mUINeedUpdateProjectInterface;
};
}
