
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
    mUIGlobals.mNeedUpdateProjectWindow = true;
    mProject.Open( projectPath );
}


void BubbleEditor::OnUpdate()
{
    // Run game
    if ( mEditorMode == EditorMode::Editing and
         mWindow.GetWindowInput().IsKeyClicked( KeyboardKey::F5 ) )
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
         mWindow.GetWindowInput().IsKeyClicked( KeyboardKey::F6 ) )
    {
        mEditorMode = EditorMode::Editing;
        mProject.mScene = mSceneSave;
    }


    // Manage selection
    if ( mEditorMode == EditorMode::Editing and
         mWindow.GetWindowInput().IsKeyClicked( KeyboardKey::DEL ) )
    {
        
    }



    // Save project
    if ( mWindow.GetWindowInput().IsKeyClicked( KeyboardKey::S ) and
         mWindow.GetWindowInput().KeyMods().CONTROL and
         mEditorMode == EditorMode::Editing )
    {
        if ( mProject.IsValid() )
            mProject.Save();
    }

}



} // namespace bubble
