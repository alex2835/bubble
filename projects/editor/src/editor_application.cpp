
#include "editor_application/editor_application.hpp"
#include <sol/sol.hpp>
#include <nlohmann/json.hpp>
#include "engine/scene/components/state_component.hpp"

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
      
      mEditorLua( OperatorContext{ mProject, mHistory, mSelection, mClipboard }, mOperatorQueue ),
      mAutoBackup( mProject, 5.0f ), // Backup every 5 minutes
      mProjectResourcesHotReloader( mProject, mUIGlobals ),
      mEditorUserInterface( *this )
{
    mWindow.SetVSync( false );
    mProject.mScriptingEngine.SetCurrentState();
    OperatorRegistry::RegisterBuiltins();
    RegisterEditorOperators();

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
                // Sounds previewed from the inspector are heard from here.
                mEngine.PropagateEditorAudio( mProject.mLevel.mScene );
                // Clips play in the editor too; that is how one is previewed.
                mEngine.UpdateAnimations( mProject.mLevel.mScene, deltaTime.Seconds() );
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
                if ( mUIGlobals.mDrawSkeletons )
                    mEngine.DrawSkeletons( mSceneViewport, mProject.mLevel.mScene );
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

    // Before any window draws this frame: what was queued last frame may
    // replace the level or the project the windows are showing.
    OperatorContext ctx = Operators();
    mOperatorQueue.Flush( ctx );

    // Whatever ran this frame - a hotkey, a queued operator, a script -
    // must not leave the windows a selection that points at nothing.
    if ( mProject.IsValid() )
        mSelection.Prune( mProject.mLevel.mScene, mProject.mLevel.mTreeRoot );
}

void BubbleEditor::RegisterEditorOperators()
{
    auto& registry = OperatorRegistry::Instance();
    if ( registry.Find( "project.open" ) )
        return;

    // The editor's own verbs, on top of the engine's: they touch the
    // editor's state (mode, windows), so they are registered here rather
    // than in the engine. Blender keeps these under wm.* for the same reason.
    const auto editing = [this]( const OperatorContext&, const json& ) { return mEditorMode == EditorMode::Editing; };
    const auto running = [this]( const OperatorContext&, const json& ) { return mEditorMode == EditorMode::Running; };
    const auto projectOpen = [this]( const OperatorContext&, const json& )
    {
        return mEditorMode == EditorMode::Editing and mProject.IsValid();
    };

    // args: path (the .bubble file)
    registry.Register( { "project.open", "Open project", editing,
        [this]( OperatorContext&, const json& args ) { OpenProject( path( args.at( "path" ).get<string>() ) ); } } );
    registry.Register( { "project.save", "Save project", projectOpen,
        [this]( OperatorContext&, const json& ) { mProject.Save(); } } );

    // args: file (relative to the project root, as Project::Levels() lists)
    registry.Register( { "level.open", "Open level", projectOpen,
        [this]( OperatorContext&, const json& args )
        {
            const path file = path( args.at( "file" ).get<string>() );
            if ( file == mProject.CurrentLevel() )
                return;
            // The level being left is saved: the switch replaces it in place.
            mProject.Save();
            OpenLevel( file );
        } } );
    // args: name
    registry.Register( { "level.new", "New level", projectOpen,
        [this]( OperatorContext&, const json& args ) { NewLevel( args.at( "name" ).get<string>() ); } } );
    // args: file (default: the open level)
    registry.Register( { "level.set_startup", "Set as startup level", projectOpen,
        [this]( OperatorContext&, const json& args )
        {
            mProject.mStartupLevel = args.contains( "file" ) ? path( args.at( "file" ).get<string>() )
                                                             : mProject.CurrentLevel();
        } } );

    // args: window ("animation_graph"), show (default true). Windows that
    // start closed; the menu's checkbox goes through the same flag.
    registry.Register( { "window.show", "Show window", nullptr,
        [this]( OperatorContext&, const json& args )
        {
            const string window = args.at( "window" ).get<string>();
            const bool show = args.value( "show", true );
            if ( window == "animation_graph" )
                mUIGlobals.mShowAnimationGraph = show;
            else
                throw std::runtime_error( std::format( "window.show: no window '{}'", window ) );
        } } );

    registry.Register( { "game.run", "Run", projectOpen,
        [this]( OperatorContext&, const json& )
        {
            try
            {
                mEditorMode = EditorMode::Running;
                StartEngine();
            }
            catch ( ... )
            {
                mEditorMode = EditorMode::Editing;
                StopEngine();
                throw;
            }
        } } );
    registry.Register( { "game.stop", "Stop", running,
        [this]( OperatorContext&, const json& )
        {
            mEditorMode = EditorMode::Editing;
            StopEngine();
        } } );
}

void BubbleEditor::OnUpdateHotKeys()
{
    const auto& input = mWindow.GetWindowInput();
    const bool ctrlPressed = input.KeyMods().CONTROL;

    // Run / stop the game
    if ( input.IsKeyClicked( KeyboardKey::F5 ) )
        Invoke( "game.run" );
    if ( input.IsKeyClicked( KeyboardKey::F6 ) )
        Invoke( "game.stop" );

    // Editing hotkeys. Each one is an operator; the key only names it.
    if ( mEditorMode == EditorMode::Editing )
    {
        static constexpr std::pair<KeyboardKey, const char*> ctrlKeys[] = {
            { KeyboardKey::S, "project.save" },
            { KeyboardKey::Z, "history.undo" },
            { KeyboardKey::Y, "history.redo" },
            { KeyboardKey::X, "scene.cut" },
            { KeyboardKey::C, "scene.copy" },
            { KeyboardKey::V, "scene.paste" },
        };
        static constexpr std::pair<KeyboardKey, const char*> plainKeys[] = {
            { KeyboardKey::DEL, "scene.delete" },
        };

        for ( const auto& [key, op] : ctrlKeys )
            if ( ctrlPressed and input.IsKeyClicked( key ) )
                Invoke( op );
        for ( const auto& [key, op] : plainKeys )
            if ( not ctrlPressed and input.IsKeyClicked( key ) )
                Invoke( op );
    }
}

void BubbleEditor::RunScript( const path& file )
{
    mEditorLua.RunFile( file );
}

bool BubbleEditor::Invoke( const char* op, const json& args )
{
    try
    {
        OperatorContext ctx = Operators();
        return InvokeOperator( op, ctx, args );
    }
    catch ( const std::exception& e )
    {
        LogError( "{}: {}", op, e.what() );
        return false;
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
