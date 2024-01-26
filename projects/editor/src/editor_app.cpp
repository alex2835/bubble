
#include "app/editor_app.hpp"
#include "engine/utils/emscripten_main_loop.hpp"
#include "interface/scnene_viewport_interface.hpp"
#include <functional>

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
    //uniform mat4 uProjection;
    //uniform mat4 uView;

    uniform mat4 uModel;

    void main()
    {
        gl_Position = uProjection * uView * uModel * vec4(aPosition, 1.0);
    }
)shader";

constexpr std::string_view frag_shader = R"shader(
    #version 330 core
    out vec4 FragColor;
    
    uniform vec4 uColor;
    void main()
    {
        FragColor = vec4(1. );
    }
)shader";


BubbleEditor::BubbleEditor()
    : mWindow( "Bubble", WindowSize{ 1200, 720 } ),
      mSceneViewport( { 640, 800 } ),
      mInterfaceLoader( mWindow.GetImGuiContext() )
{
    glewInit();
    ImGui::SetCurrentContext( mWindow.GetImGuiContext() );

    auto editorViewportInterface = Ref<IEditorInterface>( ( IEditorInterface* ) new SceneViewportInterface( mSceneViewport ) );
    mInterfaceLoader.AddInterface( editorViewportInterface );
    mInterfaceLoader.LoadInterfaces();
}

void BubbleEditor::Run()
{
    path model_path = R"(C:\Users\sa007\Desktop\projects\Bubble0.5\test_project\resources\models\crysis\nanosuit.obj)";
    auto model = mEngine.mLoader.LoadModel( model_path );
    auto shader = mEngine.mLoader.LoadShader( "test_shader", vert_shader, frag_shader );
    model->mShader = shader;

    Entity entity = mEngine.mScene.CreateEntity();
    entity.AddComponet<ModelComponent>( model )
          .AddComponet<TransformComponent>( mat4( 1.0f ) );


#ifdef __EMSCRIPTEN__
    EMSCRIPTEN_MAINLOOP_BEGIN
#else
    while ( !mWindow.ShouldClose() )
#endif
    {
        // Events
        const auto& events = mWindow.PollEvents();
        for ( const auto& event : events )
            mSceneCamera.OnEvent( event );

        // Update
        mEngine.OnUpdate();
        auto dt = mEngine.mTimer.GetDeltaTime();

        mInterfaceLoader.OnUpdate( dt );
        mSceneCamera.OnUpdate( dt );

        // Draw 
        mEngine.DrawScene( mSceneCamera, mSceneViewport );


        Framebuffer::BindWindow( mWindow );
        mWindow.ImGuiBegin();
        ImGui::ShowDemoWindow();
        mInterfaceLoader.OnDraw();
        mWindow.ImGuiEnd();

        mWindow.OnUpdate();
    }
#ifdef __EMSCRIPTEN__
    EMSCRIPTEN_MAINLOOP_END;
#endif
}


}
