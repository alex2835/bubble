
#include "editor_application/editor_application.hpp"
#include <sol/sol.hpp>
#include <print>

namespace bubble
{
constexpr uvec2 WINDOW_SIZE{ 1920, 1080 };
constexpr uvec2 VIEWPORT_SIZE{ 800, 640 };

BubbleEditor::BubbleEditor()
    : mWindow( Window( "Bubble", WINDOW_SIZE ) ),
      mEditorMode( EditorMode::Editing ),
      mSceneViewport( Framebuffer( Texture2DSpecification::CreateRGBA8( VIEWPORT_SIZE ),
                                   Texture2DSpecification::CreateDepth( VIEWPORT_SIZE ) ) ),
      mObjectIdViewport( Framebuffer( Texture2DSpecification::CreateObjectId( VIEWPORT_SIZE ),
                                      Texture2DSpecification::CreateDepth( VIEWPORT_SIZE ) ) ),
      mObjectIdShader( LoadShader( OBJECT_PICKING_SHADER ) ),
      mSceneCamera( SceneCamera( mWindow.GetWindowInput(), vec3( 0, 0, 100 ) ) ),
      mProject( mWindow.GetWindowInput() ),
      mEngine( mProject ),
      mProjectResourcesHotReloader( mProject ),
      mEditorUserInterface( *this )
{
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
        mTimer.OnUpdate();
        auto deltaTime = mTimer.GetDeltaTime();

        if ( mWindow.GetWindowInput().IsKeyCliked( KeyboardKey::F5 ) 
             and mEditorMode == EditorMode::Editing )
        {
            std::println("onstart");
            mEditorMode = EditorMode::Running;
            mSceneSave = mProject.mScene;
            mEngine.OnStart();
        }

        if ( mWindow.GetWindowInput().IsKeyCliked( KeyboardKey::F6 )
             and mEditorMode == EditorMode::Running )
        {
            mProject.mScene = mSceneSave;
            mEditorMode = EditorMode::Editing;
        }


        switch ( mEditorMode )
		{
			case EditorMode::Editing:
			{
				mSceneCamera.OnUpdate( deltaTime );
				mProjectResourcesHotReloader.OnUpdate();
				mEditorUserInterface.OnUpdate( deltaTime );
				DrawProjectScene();
				break;
			}
            case EditorMode::Running:
            {
                // temp
                mSceneCamera.OnUpdate( deltaTime );
                mEngine.mActiveCamera = mSceneCamera;

                mEngine.OnUpdate();
                mEngine.DrawScene( mSceneViewport );
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
    LogInfo( "Open project {}", projectPath.string() );
    mUINeedUpdateProjectWindow = true;
    mProject.Open( projectPath );
}

void BubbleEditor::DrawProjectScene()
{
    // Draw scene
    mSceneViewport.Bind();
    mEngine.mRenderer.ClearScreen( vec4( 0.2f, 0.3f, 0.3f, 1.0f ) );
    mEngine.mRenderer.SetUniformBuffers( mSceneCamera, mSceneViewport );
    mProject.mScene.ForEach<ModelComponent, ShaderComponent, TransformComponent>(
    [&]( Entity _,
         ModelComponent& model,
         ShaderComponent& shader,
         TransformComponent& transform )
    {
        mEngine.mRenderer.DrawModel( model, transform.TransformMat(), shader );
    } );

    // Draw scene's objectId
    mObjectIdViewport.Bind();
    mEngine.mRenderer.ClearScreenUint( uvec4( 0 ) );
    mProject.mScene.ForEach<ModelComponent, TransformComponent>(
    [&]( Entity entity,
         ModelComponent& model,
         TransformComponent& transform )
    {
        mObjectIdShader->SetUni1ui( "uObjectId", (u32)entity );
        mEngine.mRenderer.DrawModel( model, transform.TransformMat(), mObjectIdShader );
    } );
}

}
