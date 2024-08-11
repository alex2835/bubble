#include "engine/scene/components_manager.hpp"
#include "engine/utils/emscripten_main_loop.hpp"
#include "engine/log/log.hpp"
#include "editor_application/editor_application.hpp"
#include <functional>

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
                                          Texture2DSpecification::CreateDepth( VIEWPORT_SIZE ) )
      },
      mEditorMode( EditorMode::Editing ),
      mResourcesHotReloader( mProject.mLoader ),
      mInterfaceHotReloader( *this )
{
    // Window
    mWindow.SetVSync( false );

    // ImGui
    ImGui::SetCurrentContext( mWindow.GetImGuiContext() );

    // Add default components
    ComponentManager::Add<TagComponent>( mProject.mScene );
    ComponentManager::Add<ModelComponent>( mProject.mScene );
    ComponentManager::Add<TransformComponent>( mProject.mScene );
    ComponentManager::Add<ShaderComponent>( mProject.mScene );

    // Selecting objects
    mObjectIdShader = LoadShader( OBJECT_PICKING_SHADER );
}


void BubbleEditor::OpenProject( const path& projectPath )
{
    LogInfo( "Openg project {}", projectPath.string() );
    mProject.Open( projectPath );
    mUINeedUpdateProjectInterface = true;
}


void BubbleEditor::Run()
{
#ifdef __EMSCRIPTEN__
    EMSCRIPTEN_MAINLOOP_BEGIN
#else
    while ( !mWindow.ShouldClose() )
#endif
    {
        // Poll events
        mTimer.OnUpdate();
        auto dt = mTimer.GetDeltaTime();
        const auto& events = mWindow.PollEvents();

        // Draw scene
        switch ( mEditorMode )
        {
        case EditorMode::Editing:
        {
            for ( const auto& event : events )
                mSceneCamera.OnEvent( event );
            mSceneCamera.OnUpdate( dt );
            mResourcesHotReloader.OnUpdate();
            mInterfaceHotReloader.OnUpdate( dt );

            DrawProjectScene();
            break;
        }
        case EditorMode::Runing:
            break;
        }
        
        // ImGui interface
        Framebuffer::BindWindow( mWindow );
        mWindow.ImGuiBegin();
        mInterfaceHotReloader.OnDraw( dt );
        mWindow.ImGuiEnd();

        // Swap window
        mWindow.OnUpdate();
    }
#ifdef __EMSCRIPTEN__
    EMSCRIPTEN_MAINLOOP_END;
#endif
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
