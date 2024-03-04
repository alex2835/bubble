
#include <functional>
#include "engine/utils/emscripten_main_loop.hpp"
#include "editor_application.hpp"
#include "editor_shaders.hpp"

namespace bubble
{
constexpr std::string_view vert_shader = R"shader(
    #version 330 core
    layout(location = 0) in vec3 aPosition;
    
    layout(std140) uniform VertexUniformBuffer
    {
        mat4 uProjection;
        mat4 uView;
    };
    uniform mat4 uModel;

    void main()
    {
        gl_Position = uProjection * uView * uModel * vec4(aPosition, 1.0);
    }
)shader";

constexpr std::string_view frag_shader = R"shader(
    #version 330 core
    out vec4 FragColor;
    void main()
    {
        FragColor = vec4(1. );
    }
)shader";

constexpr WindowSize WINDOW_SIZE{ 1200, 720 };
constexpr uvec2 VIEWPORT_SIZE{ 800, 640 };

BubbleEditor::BubbleEditor()
    : EditorState{ Window( "Bubble", WINDOW_SIZE ),
                   // Main viewport
                   Framebuffer( Texture2DSpecification::CreateRGBA8( VIEWPORT_SIZE ),
                                Texture2DSpecification::CreateDepth( VIEWPORT_SIZE ) ),
                   // Object selecting viewport
                   Framebuffer( Texture2DSpecification::CreateObjectId( VIEWPORT_SIZE ),
                                Texture2DSpecification::CreateDepth( VIEWPORT_SIZE ) ),
                   SceneCamera( vec3( 0, 15, 70 ) ) 
    },
      mEditorMode( EditorMode::Edit ),
      mInterfaceLoader( *this, mEngine )
{
    // ImGui
    ImGui::SetCurrentContext( mWindow.GetImGuiContext() );

    // Add components functions
    ComponentManager::Add<TagComponent>();
    ComponentManager::Add<TransformComponent>();
    ComponentManager::Add<ModelComponent>();

    // Selecting objects
    mObjectIdShader = Loader::JustLoadShader( "Picking shader", PICKING_VERTEX_SHADER, PICKING_FRAGMENT_SHADER );

    // Editor's viewport interface
    mInterfaceLoader.LoadInterfaces();
}


void BubbleEditor::Run()
{
    // Temp test
    path model_path = R"(C:\Users\sa007\Desktop\projects\Bubble0.5\test_project\resources\models\crysis\nanosuit.obj)";
    auto model = Loader::JustLoadModel( model_path );
    auto shader = Loader::JustLoadShader( "test_shader", vert_shader, frag_shader );
    model->mShader = shader;
    
    Entity entity = mProject.mScene.CreateEntity();
    entity.AddComponet<TagComponent>( "test" )
          .AddComponet<ModelComponent>( model )
          .AddComponet<TransformComponent>();
    mSelectedEntity = entity;

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
