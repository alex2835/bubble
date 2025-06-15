
#include "editor_application/editor_application.hpp"
#include "engine/utils/geometry.hpp"
#include <sol/sol.hpp>
#include <print>
#include <regex>

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
      mEntityIdShader( LoadShader( ENTITY_PICKING_SHADER ) ),

      mProject( mWindow.GetWindowInput() ),
      mEngine( mProject ),
      
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
                mSceneCamera.OnUpdate( deltaTime );
                mProjectResourcesHotReloader.OnUpdate();
                mEditorUserInterface.OnUpdate( deltaTime );

                mEngine.mActiveCamera = mSceneCamera;
                DrawEntityIds();
                mEngine.DrawScene( mSceneViewport, mProject.mScene );
                break;
            }
            case EditorMode::Running:
            {
                try
                {
                    // temp
                    mSceneCamera.OnUpdate( deltaTime );
                    mEngine.mActiveCamera = mSceneCamera;

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
    LogInfo( "Open project {}", projectPath.string() );
    mUIGlobals.mNeedUpdateProjectWindow = true;
    mProject.Open( projectPath );
}

void BubbleEditor::OnUpdate()
{
    /// Switch Editing/GameRunning modes
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

    if ( mEditorMode == EditorMode::Running and
         mWindow.GetWindowInput().IsKeyClicked( KeyboardKey::F6 ) )
    {
        mEditorMode = EditorMode::Editing;
        mProject.mScene = mSceneSave;
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

void BubbleEditor::DrawEntityIds()
{
    // Draw scene's entity ids to buffer
    mEntityIdViewport.Bind();
    mEngine.mRenderer.ClearScreenUint( uvec4( 0 ) );
    mProject.mScene.ForEach<ModelComponent, TransformComponent>(
        [&]( Entity entity,
             ModelComponent& modelComponent,
             TransformComponent& transformComponent )
    {
        mEntityIdShader->SetUni1ui( "uObjectId", (u32)entity );
        mEngine.mRenderer.DrawModel( modelComponent.mModel, transformComponent.TransformMat(), mEntityIdShader );
    } );
}

}
