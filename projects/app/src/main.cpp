#include <iostream>
#include <string>
#include <functional>
#include "window/window.hpp"
#include "renderer/buffer.hpp"
#include "renderer/shader.hpp"
#include "renderer/framebuffer.hpp"
#include "loader/loader.hpp"
#include "utils/emscripten_main_loop.hpp"
#include "serialization/event_serialization.hpp"

using namespace bubble;

int main()
{
    Loader loader;
    Ref<Model> teapot = loader.LoadModel("models/teapot.obj");
    if (!teapot) {
        return 1;
    }

    Window window( "Bubble", WindowSize{ 1200, 720 } );
    ImGui::SetCurrentContext( window.GetImGuiContext() );

    while ( !window.ShouldClose() )
    {
        window.PollEvents();
        
        glClearColor( 0.2f, 0.3f, 0.3f, 1.0f );
        glClear( GL_COLOR_BUFFER_BIT );

        window.ImGuiBegin();
        ImGui::ShowDemoWindow();
        window.ImGuiEnd();
        window.OnUpdate();
    }
    return 0;
}

