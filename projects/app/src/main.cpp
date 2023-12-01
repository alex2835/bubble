#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <string>

#include "window/window.hpp"
#include "window/imgui.hpp"
#include "serialization/event_serialization.hpp"

#include "renderer/buffer.hpp"
#include "renderer/shader.hpp"
#include "renderer/framebuffer.hpp"

#include "loader/loader.hpp"

using namespace bubble;

int main()
{
    Window window( "Test", WindowSize{ 1200, 720 } );
    window.SetVSync( true );
    ImGuiManager imgui( window );

    //BufferLayout layout
    //{
    //    { "Position", GLSLDataType::Float3 },
    //    { "Color", GLSLDataType::Float3  }
    //};
    //float vertices[] = {
    // 0.5f,  0.5f, 0.0f, 1.0f, 0.0f, 0.0f,  // top right
    // 0.5f, -0.5f, 0.0f, 0.0f, 1.0f, 0.0f,  // bottom right
    //-0.5f, -0.5f, 0.0f, 0.0f, 0.0f, 1.0f,  // bottom left
    //-0.5f,  0.5f, 0.0f, 1.0f, 0.0f, 0.0f,  // top left 
    //};
    //uint32_t indices[] = {  // note that we start from 0!
    //    0, 1, 3,  // first Triangle
    //    1, 2, 3   // second Triangle
    //};
    //VertexArray va;
    //va.AddVertexBuffer( VertexBuffer( layout, vertices, sizeof( vertices ) ) );
    //va.SetIndexBuffer( IndexBuffer( indices, 6 ) );

    //Loader loader;
    //std::string vertexShaderSource = "#version 330 core\n"
    //    "layout (location = 0) in vec3 aPos;\n"
    //    "layout (location = 1) in vec3 aColor;\n"
    //    "out vec3 Color;\n"
    //    "void main()\n" 
    //    "{\n"
    //    "   gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0);\n"
    //    "   Color = aColor;\n"
    //    "}\0";
    //std::string fragmentShaderSource = "#version 330 core\n"
    //    "in vec3 Color;\n"
    //    "out vec4 FragColor;\n"
    //    "void main()\n"
    //    "{\n"
    //    "   FragColor = vec4(Color, 1.0f);\n"
    //    "}\n\0";

    //auto shader = loader.LoadShader( "basic",
    //                                 vertexShaderSource,
    //                                 fragmentShaderSource );

    while ( !window.ShouldClose() )
    {
        window.PollEvents();
        window.OnUpdate();

        //glClearColor( 0.2f, 0.3f, 0.3f, 1.0f );
        //glClear( GL_COLOR_BUFFER_BIT );
        //shader->Bind();
        //va.Bind();
        //glDrawElements( GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr );
    
        imgui.Begin();
        imgui.End();
    }
    return 0;
}