
#include "app/editor_app.hpp"
#include "engine/utils/emscripten_main_loop.hpp"
#include <functional>


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
        FragColor = vec4(1.0);
    }
)shader";



namespace bubble
{
BubbleEditor::BubbleEditor() 
    : mWindow( "Bubble", WindowSize{ 1200, 720 } )
{
    ImGui::SetCurrentContext( mWindow.GetImGuiContext() );
}

void BubbleEditor::Run()
{
    std::path model_path = R"(C:\Users\sa007\Desktop\projects\Bubble0.5\test_project\resources\models\crysis\nanosuit.obj)";
    auto model = loader.LoadModel( model_path );
    auto shader = loader.LoadShader( "test_shader", vert_shader, frag_shader );
    model->mShader = shader;

#ifdef __EMSCRIPTEN__
    EMSCRIPTEN_MAINLOOP_BEGIN
#else
    while ( !mWindow.ShouldClose() )
#endif
    {

        // Events
        const auto& events = mWindow.PollEvents();
        for ( const auto& event : events )
        {
            mSceneCamera.OnEvent( event );
        }
        // Update
        mTimer.OnUpdate();
        mSceneCamera.OnUpdate( mTimer.GetDeltaTime() );

        // Draw 
        //glClearColor( 0.2f, 0.3f, 0.3f, 1.0f );
        //glClear( GL_COLOR_BUFFER_BIT );

        //auto size = window.GetSize();
        //shader->SetUniMat4( "uProjection", mSceneCamera.GetPprojectionMat( size.mWidth, size.mHeight ) );
        //shader->SetUniMat4( "uView", mSceneCamera.GetLookatMat() );
        //shader->SetUniMat4( "uModel", glm::mat4( 1.0f ) );
        //mRenderer.DrawModel( model );

        mWindow.ImGuiBegin();
        ImGui::ShowDemoWindow();
        mWindow.ImGuiEnd();
        mWindow.OnUpdate();
    }
#ifdef __EMSCRIPTEN__
    EMSCRIPTEN_MAINLOOP_END;
#endif
}


}
