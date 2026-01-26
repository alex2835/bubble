
#include "editor_application/editor_application.hpp"

namespace bubble
{
constexpr uvec2 WINDOW_SIZE{ 1920, 1080 };
constexpr uvec2 VIEWPORT_SIZE{ 800, 640 };

BubbleEditor::BubbleEditor()
    : mWindow( Window( "Bubble", WINDOW_SIZE ) ),
      mEngine( mWindow ),
      mEditorMode( EditorMode::Editing ),
      mSceneCamera( SceneCamera( mWindow.GetWindowInput(), vec3( 0, 0, 100 ) ) ),

      mSceneViewport( Framebuffer( Texture2DSpecification::CreateRGBA8( VIEWPORT_SIZE ),
                                   Texture2DSpecification::CreateDepth( VIEWPORT_SIZE ) ) ),

      mEntityIdViewport( Framebuffer( Texture2DSpecification::CreateObjectId( VIEWPORT_SIZE ),
                                      Texture2DSpecification::CreateDepth( VIEWPORT_SIZE ) ) ),
      
      mAutoBackup( mProject, 5.0f ), // Backup every 5 minutes
      mProjectResourcesHotReloader( mProject ),
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
                    mSceneCamera.OnUpdate( deltaTime );
                mEngine.mActiveCamera = (Camera)mSceneCamera;

                // Draw project scene
                mEngine.PropagateTransforms( mProject.mScene );
                mEngine.DrawScene( mSceneViewport, mProject.mScene );
                mEngine.DrawEditorBillboards( mSceneViewport, mProject.mScene );
                mEngine.DrawEntityIds( mEntityIdViewport, mProject.mScene );

                // Update editor helpers
                mProjectResourcesHotReloader.OnUpdate();
                mAutoBackup.OnUpdate( deltaTime );
                mEditorUserInterface.OnUpdate( deltaTime );

                // Bounding boxes and physics shape
                if ( mUIGlobals.mDrawBoundingBoxes )
                    mEngine.DrawBoundingBoxes( mSceneViewport, mProject.mScene );
                if ( mUIGlobals.mDrawPhysicsShapes )
                    mEngine.DrawPhysicsShapes( mSceneViewport, mProject.mScene );
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
                };
                break;
            }
        }
        mWindow.ImGuiBegin();
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
            mEngine.mScene = mProject.mScene;
            mEngine.mLoader = mProject.mLoader;
            mEngine.OnStart();
        }
        catch ( const std::exception& e )
        {
            mEditorMode = EditorMode::Editing;
            LogError( e.what() );
        };
    }

    // Stop game
    if ( mEditorMode == EditorMode::Running and
         input.IsKeyClicked( KeyboardKey::F6 ) )
    {
        mEditorMode = EditorMode::Editing;
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

        // Ctrl+Z - Undo
        if ( ctrlPressed and input.IsKeyClicked( KeyboardKey::Z ) )
        {
            mHistory.Undo();
        }

        // Ctrl+Y - Redo
        if ( ctrlPressed and input.IsKeyClicked( KeyboardKey::Y ) )
        {
            mHistory.Redo();
        }

        // Del - Delete selection
        if ( input.IsKeyClicked( KeyboardKey::DEL ) )
        {
            if ( not mSelection.IsEmpty() )
            {
                if ( mSelection.GetTreeNode() )
                {
                    // Single node deletion (from tree hierarchy)
                    auto nodeToRemove = mSelection.GetTreeNode();
                    mSelection.Clear();

                    auto command = std::make_unique<DeleteNodeCommand>(
                        nodeToRemove,
                        mProject.mScene
                    );
                    mHistory.ExecuteCommand( std::move( command ) );
                }
                else
                {
                    // Multiple entities selected (viewport selection)
                    // Find all nodes corresponding to selected entities
                    vector<Ref<ProjectTreeNode>> nodesToDelete;
                    for ( auto entity : mSelection.GetEntities() )
                    {
                        auto node = FindNodeByEntity( entity, mProject.mProjectTreeRoot );
                        if ( node )
                            nodesToDelete.push_back( node );
                    }

                    if ( not nodesToDelete.empty() )
                    {
                        mSelection.Clear();

                        auto command = std::make_unique<DeleteMultipleNodesCommand>(
                            nodesToDelete,
                            mProject.mScene,
                            mProject.mProjectTreeRoot
                        );
                        mHistory.ExecuteCommand( std::move( command ) );
                    }
                }
            }
        }

        // Ctrl+X - Cut
        if ( ctrlPressed and input.IsKeyClicked( KeyboardKey::X ) )
        {
            if ( not mSelection.IsEmpty() and mSelection.GetTreeNode() )
            {
                mClipboard.Cut( mSelection.GetTreeNode() );
                mSelection.Clear();
            }
        }

        // Ctrl+C - Copy
        if ( ctrlPressed and input.IsKeyClicked( KeyboardKey::C ) )
        {
            if ( not mSelection.IsEmpty() and mSelection.GetTreeNode() )
            {
                mClipboard.Copy( mSelection.GetTreeNode() );
            }
        }

        // Ctrl+V - Paste (move if cut, copy if copied)
        if ( ctrlPressed and input.IsKeyClicked( KeyboardKey::V ) )
        {
            if ( not mClipboard.IsEmpty() )
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
                    if ( mClipboard.IsCut() )
                    {
                        // Move: execute move command through history
                        auto command = std::make_unique<MoveNodeCommand>( mClipboard.GetNode(), targetParent );
                        mHistory.ExecuteCommand( std::move( command ) );
                        mClipboard.Clear();
                    }
                    else
                    {
                        // Copy: execute copy command through history
                        auto command = std::make_unique<CopyNodeCommand>( mClipboard.GetNode(), targetParent, mProject.mScene );
                        mHistory.ExecuteCommand( std::move( command ) );
                    }
                }
            }
        }
    }
}

} // namespace bubble
