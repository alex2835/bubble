#include "engine/utils/emscripten_main_loop.hpp"
#include "editor_application.hpp"
#include "editor_shaders.hpp"
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
      mEditorMode( EditorMode::Edit ),
      mInterfaceLoader( *this, mEngine )
{
    // Add components functions
    ComponentManager::Add<TagComponent>();
    ComponentManager::Add<ModelComponent>();
    ComponentManager::Add<TransformComponent>();

    // ImGui
    ImGui::SetCurrentContext( mWindow.GetImGuiContext() );

    // Selecting objects
    mObjectIdShader = Loader::JustLoadShader( OBJECT_PICKING_SHADER );

    // Editor's viewport interface
    mInterfaceLoader.LoadInterfaces();
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

        // Update engine
        mEngine.OnUpdate();
        auto dt = mEngine.mTimer.GetDeltaTime();
        mSceneCamera.OnUpdate( dt );
        mInterfaceLoader.OnUpdate( dt );

        // Draw scene
        switch ( mEditorMode )
        {
        case EditorMode::Edit:
            SetUniformBuffer();
            DrawProjectScene();
            DrawSceneObjectId();
            break;
        case EditorMode::Run:
            break;
        }
        
        // ImGui interface
        Framebuffer::BindWindow( mWindow );
        mWindow.ImGuiBegin();
        mInterfaceLoader.OnDraw( dt );
        mWindow.ImGuiEnd();

        // Render window
        mWindow.OnUpdate();
    }
#ifdef __EMSCRIPTEN__
    EMSCRIPTEN_MAINLOOP_END;
#endif
}


void BubbleEditor::SetUniformBuffer()
{
    const auto& camera = mSceneCamera;
    auto size = mSceneViewport.Size();

    auto vertexBufferElement = mEngine.mRenderer.mVertexUniformBuffer->Element( 0 );
    vertexBufferElement.SetMat4( "uProjection", camera.GetPprojectionMat( size.x, size.y ) );
    vertexBufferElement.SetMat4( "uView", camera.GetLookatMat() );

    auto fragmentBufferElement = mEngine.mRenderer.mLightsInfoUniformBuffer->Element( 0 );
    fragmentBufferElement.SetInt( "uNumLights", 0 );
    fragmentBufferElement.SetFloat3( "uViewPos", camera.mPosition );
}

void BubbleEditor::DrawProjectScene()
{
    mSceneViewport.Bind();
    mEngine.mRenderer.ClearScreen( vec4( 0.2f, 0.3f, 0.3f, 1.0f ) );
    mProject.mScene.ForEach<ModelComponent, TransformComponent>(
    [&]( Entity entity,
         ModelComponent& model,
         TransformComponent& transform )
    {
        mEngine.mRenderer.DrawModel( model, transform.Transform(), model->mShader );
    } );
}

void BubbleEditor::DrawSceneObjectId()
{
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
