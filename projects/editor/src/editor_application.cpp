
#include "editor_application/editor_application.hpp"
#include <sol/sol.hpp>

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
      mProjectResourcesHotReloader( mProject, mUIGlobals ),
      mEditorUserInterface( *this )
{
    mWindow.SetVSync( false );
    mProject.mScriptingEngine.SetCurrentState();

    mEditorSettings.Load();
    mEditorSettings.Apply( mWindow, mSceneCamera, mUIGlobals );

    // The window is created hidden, show it only once it sits where the last
    // session left it.
    mWindow.Show();
}


BubbleEditor::~BubbleEditor()
{
    mEditorSettings.Capture( mWindow, mSceneCamera, mUIGlobals );
    mEditorSettings.Save();
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
        const auto deltaTime = mTimer.GetDeltaTime();

        // One ring of per draw uniforms is handed out across every pass this
        // frame, so it resets once here.
        mEngine.mRenderer.BeginFrame();

        switch ( mEditorMode )
        {
            case EditorMode::Editing:
            {
                // Update scene camera
                if ( not mUIGlobals.mIsViewManipulatorUsing )
                    mSceneCamera.OnUpdate( deltaTime );
                mEngine.mCamera = (Camera)mSceneCamera;

                // Draw project scene
                mEngine.PropagateCameraTransforms( mProject.mLevel.mScene );
                mEngine.PropagateLightTransforms( mProject.mLevel.mScene );
                mEngine.DrawScene( mSceneViewport, mProject.mLevel.mScene );
                mEngine.DrawEditorBillboards( mSceneViewport, mProject.mLevel.mScene );

                // Only on frames where something asked to pick. This used to run
                // unconditionally - a second full traversal of the scene plus a
                // billboard per camera and light, every frame, whether or not
                // anyone had clicked. It has to stay after DrawScene, which is
                // what fills the camera block it reads.
                if ( mUIGlobals.mEntityIdPicker.WantsIdPass() )
                {
                    mEngine.DrawEntityIds( mEntityIdViewport, mProject.mLevel.mScene );
                    mUIGlobals.mEntityIdPicker.CaptureFrom( mEntityIdViewport );
                }

                // Update editor helpers
                mProjectResourcesHotReloader.OnUpdate();
                mAutoBackup.OnUpdate( deltaTime );
                Validation();

                // Bounding helper lines
                mEngine.DrawCameraFrustums( mSceneViewport, mProject.mLevel.mScene );
                if ( mUIGlobals.mDrawBoundingBoxes )
                    mEngine.DrawBoundingBoxes( mSceneViewport, mProject.mLevel.mScene );
                if ( mUIGlobals.mDrawPhysicsShapes )
                    mEngine.DrawPhysicsShapes( mSceneViewport, mProject.mLevel.mScene );
                break;
            }
            case EditorMode::Running:
            {
                try
                {
                    mEngine.OnUpdate();
                    mEngine.DrawScene( mSceneViewport );
                }
                catch ( const std::exception& e )
                {
                    mEditorMode = EditorMode::Editing;
                    LogError( e.what() );
                    StopEngine();
                };
                // The id pass only runs while editing, so a read requested just
                // before entering play would never be answered.
                mUIGlobals.mEntityIdPicker.Cancel();
                break;
            }
        }
        mWindow.ImGuiBegin();
        mEditorUserInterface.OnDraw( deltaTime );
        mWindow.ImGuiEnd();
        mWindow.OnUpdate();

        // should be after all ui
        mEditorUserInterface.OnUpdate( deltaTime );
    }
#ifdef __EMSCRIPTEN__
    EMSCRIPTEN_MAINLOOP_END;
#endif
}


void BubbleEditor::OpenProject( const path& projectPath )
{
    mUIGlobals.mNeedUpdateProjectFilesWindow = true;
    mProject.Open( projectPath );
    // Project::Open opened the startup level; what pointed into the previous
    // project's level is stale the same way.
    mSelection.Clear();
    mHistory.Clear();
    mClipboard.Clear();
}

void BubbleEditor::OpenLevel( const path& relFile )
{
    // Cleared before the load, not after: the history commands hold tree nodes
    // whose id counter belongs to the level going away.
    mSelection.Clear();
    mHistory.Clear();
    mClipboard.Clear();
    mProject.OpenLevel( relFile );
}

void BubbleEditor::NewLevel( const string& name )
{
    mProject.Save();
    mSelection.Clear();
    mHistory.Clear();
    mClipboard.Clear();
    mProject.NewLevel( name );
    mUIGlobals.mNeedUpdateProjectFilesWindow = true;
}


void BubbleEditor::OnUpdate()
{
    OnUpdateHotKeys();

    // Only while editing: a running engine holds its own copy of the level.
    if ( mEditorMode != EditorMode::Editing )
        return;

    if ( mUIGlobals.mRequestOpenProject )
    {
        const path file = std::move( *mUIGlobals.mRequestOpenProject );
        mUIGlobals.mRequestOpenProject.reset();
        try
        {
            OpenProject( file );
        }
        catch ( const std::exception& e )
        {
            LogError( e.what() );
        }
    }

    if ( mUIGlobals.mRequestOpenLevel )
    {
        const path relFile = std::move( *mUIGlobals.mRequestOpenLevel );
        mUIGlobals.mRequestOpenLevel.reset();
        try
        {
            OpenLevel( relFile );
        }
        catch ( const std::exception& e )
        {
            LogError( e.what() );
        }
    }

    if ( mUIGlobals.mRequestNewLevel )
    {
        const string name = std::move( *mUIGlobals.mRequestNewLevel );
        mUIGlobals.mRequestNewLevel.reset();
        try
        {
            NewLevel( name );
        }
        catch ( const std::exception& e )
        {
            LogError( e.what() );
        }
    }
}


void BubbleEditor::OnUpdateHotKeys()
{
    const auto& input = mWindow.GetWindowInput();
    const bool ctrlPressed = input.KeyMods().CONTROL;

    // Start game
    if ( mEditorMode == EditorMode::Editing and
         input.IsKeyClicked( KeyboardKey::F5 ) and 
         mProject.IsValid() )
    {
        try
        {
            mEditorMode = EditorMode::Running;
            StartEngine();
        }
        catch ( const std::exception& e )
        {
            mEditorMode = EditorMode::Editing;
            LogError( e.what() );
            StopEngine();
        };
    }

    // Stop game
     if ( mEditorMode == EditorMode::Running and
         input.IsKeyClicked( KeyboardKey::F6 ) )
    {
        mEditorMode = EditorMode::Editing;
        StopEngine();
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
                        mProject.mLevel.mScene
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
                        auto node = FindNodeByEntity( entity, mProject.mLevel.mTreeRoot );
                        if ( node )
                            nodesToDelete.push_back( node );
                    }

                    if ( not nodesToDelete.empty() )
                    {
                        mSelection.Clear();

                        auto command = std::make_unique<DeleteMultipleNodesCommand>(
                            nodesToDelete,
                            mProject.mLevel.mScene,
                            mProject.mLevel.mTreeRoot
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
                    targetParent = mProject.mLevel.mTreeRoot;
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
                        auto command = std::make_unique<CopyNodeCommand>( mClipboard.GetNode(), targetParent, mProject.mLevel.mScene );
                        mHistory.ExecuteCommand( std::move( command ) );
                    }
                }
            }
        }
    }
}

void BubbleEditor::StartEngine()
{
    mProject.Save();
    mEngine.mProject.mLoader = mProject.mLoader;
    // The level being edited, not the startup one: that is the one being
    // iterated on.
    mEngine.OnStart( mProject.mRootFile, mProject.CurrentLevel() );
}

void BubbleEditor::StopEngine()
{
    // Engine::OnEnd releases it too, but that runs after code that can throw and
    // an editor left without a cursor cannot be clicked out of.
    mWindow.LockCursor( false );

    mEngine.OnEnd();
    mProject.mScriptingEngine.SetCurrentState();
}

void BubbleEditor::Validation()
{
    /// Validate editor state

    // Check scene's state components binded to right LuaVM
    mProject.mLevel.mScene.ForEach<StateComponent>( [&]( Entity, StateComponent& c ) {
        BUBBLE_ASSERT( mProject.mScriptingEngine.mLua->lua_state() == c.mState->as<Table>().lua_state(), "Wrong lua binded" );
    } );
}

} // namespace bubble
