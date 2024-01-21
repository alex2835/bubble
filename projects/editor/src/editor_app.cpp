
#include "app/editor_app.hpp"
#include "engine/utils/emscripten_main_loop.hpp"
#include "interface/scnene_viewport_interface.hpp"
#include <functional>

namespace bubble
{

constexpr std::string_view vert_shader = R"shader(
    #version 420 core
    layout(location = 0) in vec3 a_Position;
    
    uniform mat4 uProjection;
    uniform mat4 uView;
    uniform mat4 uModel;

    void main()
    {
        gl_Position = uProjection * uView * uModel * vec4(a_Position, 1.0);
    }
)shader";

constexpr std::string_view frag_shader = R"shader(
    #version 420 core
    out vec4 FragColor;
    
    uniform vec4 u_Color;
    
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
    ImGui::SetCurrentContext( mWindow.GetImGuiContext() );
    
    auto editorViewportInterface = Ref<IEditorInterface>( ( IEditorInterface* ) new SceneViewportInterface( mSceneViewport ) );
    mInterfaceLoader.AddInterface( editorViewportInterface );
    mInterfaceLoader.LoadInterfaces();
}

void BubbleEditor::Run()
{
    //path model_path = R"(C:\Users\sa007\Desktop\projects\Bubble0.5\test_project\resources\models\crysis\nanosuit.obj)";
    //auto model = mEngine.mLoader.LoadModel( model_path );
    //auto shader = mEngine.mLoader.LoadShader( "test_shader", vert_shader, frag_shader );
    //model->mShader = shader;

    //glewInit();
    //FramebufferSpecification spec{ 640, 640 };
    //Framebuffer mSceneViewport( spec );

    //auto editorViewportInterface = Ref<IEditorInterface>(
    //    ( IEditorInterface* )new EditorViewportInterface( mSceneViewport ) );
    //mInterfaces.AddInterface( editorViewportInterface );

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
        //mSceneViewport.Bind();
        //glClearColor( 0.2f, 0.3f, 0.3f, 1.0f );
        //glClear( GL_COLOR_BUFFER_BIT );

        //auto size = mWindow.GetSize();
        //shader->SetUniMat4( "uProjection", mSceneCamera.GetPprojectionMat( size.mWidth, size.mHeight ) );
        //shader->SetUniMat4( "uView", mSceneCamera.GetLookatMat() );
        //shader->SetUniMat4( "uModel", mat4( 1.0f ) );
        //mEngine.mRenderer.DrawModel( model );

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
