#include "engine/utils/emscripten_main_loop.hpp"
#include "engine/scene/components_manager.hpp"
#include "editor_application.hpp"
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
      mResourceHotReloader( mProject.mLoader ),
      mInterfaceHotReloader( *this, mEngine )
{
    // Window
    mWindow.SetVSync( false );
    // ImGui
    ImGui::SetCurrentContext( mWindow.GetImGuiContext() );

    // Add default components
    ComponentManager::SetScene( mProject.mScene );
    ComponentManager::Add<TagComponent>();
    ComponentManager::Add<ModelComponent>();
    ComponentManager::Add<TransformComponent>();
    ComponentManager::Add<ShaderComponent>();

    // Selecting objects
    mObjectIdShader = LoadShader( OBJECT_PICKING_SHADER );

    // Editor's viewport interface
    mInterfaceHotReloader.LoadInterfaces();
}


void BubbleEditor::Run()
{
    mProject.Open( R"(C:\Users\sa007\Desktop\projects\bubble_sand_box\project name\project name.bubble)" );

#ifdef __EMSCRIPTEN__
    EMSCRIPTEN_MAINLOOP_BEGIN
#else
    while ( !mWindow.ShouldClose() )
#endif
    {
        // Poll events
        const auto& events = mWindow.PollEvents();
        for ( const auto& event : events )
            mSceneCamera.OnEvent( event );

        // Update editor state
        mTimer.OnUpdate();
        auto dt = mTimer.GetDeltaTime();
        mSceneCamera.OnUpdate( dt );
        mResourceHotReloader.OnUpdate();
        mInterfaceHotReloader.OnUpdate( dt );

        // Draw scene
        switch ( mEditorMode )
        {
        case EditorMode::Editing:
            DrawProjectScene();
            break;
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
