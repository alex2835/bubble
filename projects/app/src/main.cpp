#include <iostream>
#include <string>
#include <functional>
#include "window/window.hpp"
#include "renderer/buffer.hpp"
#include "renderer/shader.hpp"
#include "renderer/camera_free.hpp"
#include "renderer/framebuffer.hpp"
#include "renderer/renderer.hpp"
#include "loader/loader.hpp"
#include "utils/emscripten_main_loop.hpp"
#include "serialization/event_serialization.hpp"

#include "scene_camera.hpp"

using namespace bubble;


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


int main()
{
    Window window( "Bubble", WindowSize{ 1200, 720 } );
    ImGui::SetCurrentContext( window.GetImGuiContext() );
   
    glewInit();
    Timer timer;
    SceneCamera camera;
    Renderer renderer;

    Loader loader;
    std::path model_path = R"(C:\Users\sa007\Desktop\projects\Bubble0.5\test_project\resources\models\crysis\nanosuit.obj)";
    auto model = loader.LoadModel( model_path );
    auto shader = loader.LoadShader( "test_shader", vert_shader, frag_shader );
    model->mShader = shader;

#ifdef __EMSCRIPTEN__
    EMSCRIPTEN_MAINLOOP_BEGIN
#else
    while ( !window.ShouldClose() )
#endif
    {
        // Events
        const auto& events = window.PollEvents();
        for ( const auto& event : events )
        {
            std::cout << ToString( event ) << "\n";
            camera.OnEvent( event );
        }
        // Update
        timer.OnUpdate();
        camera.OnUpdate( timer.GetDeltaTime() );

        // Draw 
        glClearColor( 0.2f, 0.3f, 0.3f, 1.0f );
        glClear( GL_COLOR_BUFFER_BIT );

        auto size = window.GetSize();
        shader->SetUniMat4( "uProjection", camera.GetPprojectionMat( size.mWidth, size.mHeight ) );
        shader->SetUniMat4( "uView", camera.GetLookatMat() );
        shader->SetUniMat4( "uModel", glm::mat4( 1.0f ) );
        renderer.DrawModel( model );

        //window.ImGuiBegin();
        //ImGui::ShowDemoWindow();
        //window.ImGuiEnd();
        window.OnUpdate();
    }
#ifdef __EMSCRIPTEN__
    EMSCRIPTEN_MAINLOOP_END;
#endif
    return 0;
}

