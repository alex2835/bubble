#include "window.hpp"

namespace bubble
{
Window::Window( const std::string& name, WindowSize size )
{
    if( !glfwInit() )
        throw std::runtime_error( "GLFW init error" );

    mWindow = glfwCreateWindow( size.mWidth, size.mHeight, name.c_str(), NULL, NULL );
    if( !mWindow )
    {
        glfwTerminate();
        throw std::runtime_error( "GLFW window creation error" );
    }
    glfwMakeContextCurrent( mWindow );

    GLenum err = glewInit();
    if( GLEW_OK != err )
    {
        std::cerr << "Error: " << glewGetErrorString( err ) << std::endl;
        glfwTerminate();
        throw std::runtime_error( "GLEW init error" );
    }
}

Window::~Window()
{
    glfwTerminate();
}

bool Window::ShouldClose()
{
    return mShouldClose;
}

WindowSize Window::GetSize()
{
    return mWindowSize;
}

void Window::PollEvents()
{
    glfwPollEvents();
    mShouldClose = glfwWindowShouldClose( mWindow );
}

void Window::OnUpdate()
{
    glfwSwapBuffers( mWindow );
}

void Window::SetVSync( bool vsync )
{
    glfwSwapInterval( vsync );
}

}