
#include "editor_application/editor_application.hpp"

namespace bubble
{
constexpr WindowSize WINDOW_SIZE{ 1200, 720 };
constexpr uvec2 VIEWPORT_SIZE{ 800, 640 };

BubbleEditor::BubbleEditor()
    : EditorState{
        .mWindow = Window( "Bubble", WINDOW_SIZE ),
        .mSceneViewport = Framebuffer( Texture2DSpecification::CreateRGBA8( VIEWPORT_SIZE ),
                                       Texture2DSpecification::CreateDepth( VIEWPORT_SIZE ) ),
        .mObjectIdViewport = Framebuffer( Texture2DSpecification::CreateObjectId( VIEWPORT_SIZE ),
                                          Texture2DSpecification::CreateDepth( VIEWPORT_SIZE ) ),
        .mSceneCamera = SceneCamera( mWindow.GetWindowInput() )
      },
      mEditorMode( EditorMode::Editing ),
      mResourcesHotReloader( mProject.mLoader ),
      mEditorUserInterface( *this ),
      mObjectIdShader( LoadShader( OBJECT_PICKING_SHADER ) )
{
    AddDefaultComponents();
}


void BubbleEditor::Run()
{
    ScriptingEngine lua( mProject.mScene );
    Script script( "C:/Users/avusc/Desktop/projects/bubble_sand_box/bubble_project/scripts/main.lua" );

    lua.RunScript( script );

    //exit(0);



#ifdef __EMSCRIPTEN__
    EMSCRIPTEN_MAINLOOP_BEGIN
#else
    while ( !mWindow.ShouldClose() )
#endif
    {
        // Poll events
        mWindow.PollEvents();
        mTimer.OnUpdate();
        auto deltaTime = mTimer.GetDeltaTime();

        // Draw scene
        switch ( mEditorMode )
		{
			case EditorMode::Editing:
			{
				mSceneCamera.OnUpdate( deltaTime );
				mResourcesHotReloader.OnUpdate();
				mEditorUserInterface.OnUpdate( deltaTime );

				DrawProjectScene();
                // ImGui
                Framebuffer::BindWindow( mWindow );
                mWindow.ImGuiBegin();
                mEditorUserInterface.OnDraw( deltaTime );
                mWindow.ImGuiEnd();
				break;
			}
			case EditorMode::Runing:
				break;
        }
        mWindow.OnUpdate();
    }
#ifdef __EMSCRIPTEN__
    EMSCRIPTEN_MAINLOOP_END;
#endif
}


void BubbleEditor::AddDefaultComponents()
{
    ComponentManager::Add<TagComponent>( mProject.mScene );
    ComponentManager::Add<ModelComponent>( mProject.mScene );
    ComponentManager::Add<TransformComponent>( mProject.mScene );
    ComponentManager::Add<ShaderComponent>( mProject.mScene );
}


void BubbleEditor::OpenProject( const path& projectPath )
{
    LogInfo( "Open project {}", projectPath.string() );
    mUINeedUpdateProjectWindow = true;
    mProject.Open( projectPath );
}


void BubbleEditor::DrawProjectScene()
{
    mEngine.mRenderer.SetUniformBuffers( mSceneCamera, mSceneViewport );

    // Draw scene
    mSceneViewport.Bind();
    mEngine.mRenderer.ClearScreen( vec4( 0.2f, 0.3f, 0.3f, 1.0f ) );
    mProject.mScene.ForEach<ModelComponent, ShaderComponent, TransformComponent>(
    [&]( Entity entity,
         ModelComponent& model,
         ShaderComponent& shader,
         TransformComponent& transform )
    {
        mEngine.mRenderer.DrawModel( model, transform.Transform(), shader );
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
        mEngine.mRenderer.DrawModel( model, transform.Transform(), mObjectIdShader );
    } );
}

}
