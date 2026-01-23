
#include "editor_application/editor_application.hpp"
#include <print>

namespace bubble
{
constexpr uvec2 WINDOW_SIZE{ 1920, 1080 };
constexpr uvec2 VIEWPORT_SIZE{ 800, 640 };

BubbleEditor::BubbleEditor()
    : mWindow( Window( "Bubble", WINDOW_SIZE ) ),
      mEditorMode( EditorMode::Editing ),
      mSceneCamera( SceneCamera( mWindow.GetWindowInput(), vec3( 0, 0, 100 ) ) ),

      mSceneViewport( Framebuffer( Texture2DSpecification::CreateRGBA8( VIEWPORT_SIZE ),
                                   Texture2DSpecification::CreateDepth( VIEWPORT_SIZE ) ) ),

      mEntityIdViewport( Framebuffer( Texture2DSpecification::CreateObjectId( VIEWPORT_SIZE ),
                                      Texture2DSpecification::CreateDepth( VIEWPORT_SIZE ) ) ),

      mProject( mWindow.GetWindowInput() ),
      mProjectResourcesHotReloader( mProject ),
      mEngine( mProject ),

      mEditorUserInterface( *this )
{
    mWindow.SetVSync( false );
}


void BubbleEditor::Run()
{
#ifdef __EMSCRIPTEN__
    EMSCRIPTEN_MAINLOOP_BEGIN
#else
    while ( !mWindow.ShouldClose() )
#endif
    {
        mWindow.PollEvents();
        OnUpdate();
        mTimer.OnUpdate();
        auto deltaTime = mTimer.GetDeltaTime();

        switch ( mEditorMode )
        {
            case EditorMode::Editing:
            {
                if ( not mUIGlobals.mIsViewManipulatorUsing )
                {
                    mSceneCamera.OnUpdate( deltaTime );
                    //std::println("kekeke");
                }

                //std::println("Y{}, P{}", mSceneCamera.mYaw, mSceneCamera.mPitch );
                

                mProjectResourcesHotReloader.OnUpdate();
                mEditorUserInterface.OnUpdate( deltaTime );

                mEngine.mActiveCamera = (Camera)mSceneCamera;
                mEngine.DrawScene( mSceneViewport, mProject.mScene );
                mEngine.DrawEditorBillboards( mSceneViewport, mProject.mScene );
                mEngine.DrawEntityIds( mEntityIdViewport, mProject.mScene );
                break;
            }
            case EditorMode::Running:
            {
                try
                {
                    // temp
                    mSceneCamera.OnUpdate( deltaTime );
                    mEngine.mActiveCamera = (Camera)mSceneCamera;

                    mEngine.OnUpdate();
                    mEngine.DrawScene( mSceneViewport );
                }
                catch ( const std::exception& e )
                {
                    LogError( e.what() );
                    mEditorMode = EditorMode::Editing;
                    mProject.mScene = mSceneSave;
                };
                break;
            }
        }

        if ( mUIGlobals.mDrawBoundingBoxes )
            mEngine.DrawBoundingBoxes( mSceneViewport, mProject.mScene );
        if ( mUIGlobals.mDrawPhysicsShapes )
            mEngine.DrawPhysicsShapes( mSceneViewport, mProject.mScene );

        mWindow.ImGuiBegin();
        //ImGui::ShowDemoWindow();
        mEditorUserInterface.OnDraw( deltaTime );
        mWindow.ImGuiEnd();
        mWindow.OnUpdate();
    }
#ifdef __EMSCRIPTEN__
    EMSCRIPTEN_MAINLOOP_END;
#endif
}


void BubbleEditor::OpenProject( const path& projectPath )
{
    mUIGlobals.mNeedUpdateProjectFilesWindow = true;
    mProject.Open( projectPath );
}


void BubbleEditor::OnUpdate()
{
    OnUpdateHotKeys();
}


void BubbleEditor::OnUpdateHotKeys()
{
    const auto& input = mWindow.GetWindowInput();
    const bool ctrlPressed = input.KeyMods().CONTROL;

    // Run game
    if ( mEditorMode == EditorMode::Editing and
         input.IsKeyClicked( KeyboardKey::F5 ) )
    {
        try
        {
            mEditorMode = EditorMode::Running;
            mSceneSave = mProject.mScene;
            mEngine.OnStart();
        }
        catch ( const std::exception& e )
        {
            LogError( e.what() );
            mEditorMode = EditorMode::Editing;
            mProject.mScene = mSceneSave;
        };
    }

    // Stop game
    if ( mEditorMode == EditorMode::Running and
         input.IsKeyClicked( KeyboardKey::F6 ) )
    {
        mEditorMode = EditorMode::Editing;
        mProject.mScene = mSceneSave;
    }


    // Manage selection
    if ( mEditorMode == EditorMode::Editing )
    {
        // Ctrl+S - Save project
        if ( ctrlPressed and input.IsKeyClicked( KeyboardKey::S ) )
        {
            if ( mProject.IsValid() )
                mProject.Save();
        }

        // Ctrl+Del - Delete selection
        if ( ctrlPressed and input.IsKeyClicked( KeyboardKey::DEL ) )
        {
            if ( not mSelection.IsEmpty() and mSelection.GetTreeNode() )
            {
                auto nodeToRemove = mSelection.GetTreeNode();
                mSelection.Clear();
                ProjectTreeNode::RemoveNode( nodeToRemove, mProject.mScene );
            }
        }

        // Ctrl+X - Cut
        if ( ctrlPressed and input.IsKeyClicked( KeyboardKey::X ) )
        {
            if ( not mSelection.IsEmpty() and mSelection.GetTreeNode() )
            {
                auto nodeToCut = mSelection.GetTreeNode();
                mClipboard = nodeToCut;
                mClipboardIsCut = true;
                mSelection.Clear();
            }
        }

        // Ctrl+C - Copy
        if ( ctrlPressed and input.IsKeyClicked( KeyboardKey::C ) )
        {
            if ( not mSelection.IsEmpty() and mSelection.GetTreeNode() )
            {
                // Store reference to original node for copying later
                mClipboard = mSelection.GetTreeNode();
                mClipboardIsCut = false;
            }
        }

        // Ctrl+V - Paste (move if cut, copy if copied)
        if ( ctrlPressed and input.IsKeyClicked( KeyboardKey::V ) )
        {
            if ( mClipboard )
            {
                Ref<ProjectTreeNode> targetParent;

                // Determine target parent: selected node or root
                if ( not mSelection.IsEmpty() and mSelection.GetTreeNode() )
                {
                    auto selectedNode = mSelection.GetTreeNode();
                    // If selected node is a folder/level, paste into it; otherwise paste into its parent
                    if ( not selectedNode->IsEntity() )
                        targetParent = selectedNode;
                    else
                        targetParent = selectedNode->mParent.lock();
                }
                else
                {
                    targetParent = mProject.mProjectTreeRoot;
                }

                if ( targetParent )
                {
                    if ( mClipboardIsCut )
                    {
                        // Move: remove from old parent and add to new parent
                        auto oldParent = mClipboard->mParent.lock();
                        if ( oldParent )
                        {
                            auto& oldChildren = oldParent->mChildren;
                            auto it = std::ranges::find( oldChildren, mClipboard );
                            if ( it != oldChildren.end() )
                                oldChildren.erase( it );
                        }

                        mClipboard->mParent = targetParent;
                        targetParent->mChildren.push_back( mClipboard );
                        mClipboard = nullptr;
                        mClipboardIsCut = false;
                    }
                    else
                    {
                        // Copy: create a new copy and add to parent
                        auto copiedNode = ProjectTreeNode::CopyNode( mClipboard, mProject.mScene );
                        copiedNode->mParent = targetParent;
                        targetParent->mChildren.push_back( copiedNode );
                    }
                }
            }
        }
    }
}

} // namespace bubble
