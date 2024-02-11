
#include "editor_app.hpp"
#include "engine/utils/emscripten_main_loop.hpp"
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
    : mState{ Window( "Bubble", WindowSize{ 1200, 720 } ),
              Framebuffer( { 640, 800 } ),
              SceneCamera( vec3( 0, 15, 70 ) ) },
      mInterfaceLoader( mState, mEngine, mState.window.GetImGuiContext() )
{
    ImGui::SetCurrentContext( mState.window.GetImGuiContext() );

    // Components draw ability
    ComponentsOnDrawStorage::Add<TagComponent>();
    ComponentsOnDrawStorage::Add<TransformComponent>();
    ComponentsOnDrawStorage::Add<ModelComponent>();

    // Editor's viewport interface
    mInterfaceLoader.LoadInterfaces();
}


void BubbleEditor::Run()
{
    path model_path = R"(C:\Users\sa007\Desktop\projects\Bubble0.5\test_project\resources\models\crysis\nanosuit.obj)";
    auto model = mEngine.mLoader.LoadModel( model_path );
    auto shader = mEngine.mLoader.LoadShader( "test_shader", vert_shader, frag_shader );
    model->mShader = shader;

    Entity entity = mEngine.mScene.CreateEntity();
    entity.AddComponet<TagComponent>( "test" )
          .AddComponet<ModelComponent>( model )
          .AddComponet<TransformComponent>();
    mState.selectedEntity = entity;


#ifdef __EMSCRIPTEN__
    EMSCRIPTEN_MAINLOOP_BEGIN
#else
    while ( !mState.window.ShouldClose() )
#endif
    {
        // Events
        const auto& events = mState.window.PollEvents();
        for ( const auto& event : events )
            mState.sceneCamera.OnEvent( event );

        // Update
        mEngine.OnUpdate();
        auto dt = mEngine.mTimer.GetDeltaTime();
        mState.sceneCamera.OnUpdate( dt );
        mInterfaceLoader.OnUpdate( dt );

        // Draw 
        mEngine.DrawScene( mState.sceneCamera, mState.sceneViewport );


        Framebuffer::BindWindow( mState.window );
        mState.window.ImGuiBegin();
        //ImGui::ShowDemoWindow();
        mInterfaceLoader.OnDraw( dt );
        mState.window.ImGuiEnd();

        mState.window.OnUpdate();
    }
#ifdef __EMSCRIPTEN__
    EMSCRIPTEN_MAINLOOP_END;
#endif
}


}
