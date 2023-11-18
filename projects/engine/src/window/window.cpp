#include "window/window.hpp"

namespace bubble
{

void Window::ErrorCallback( int error, const char* description )
{
    std::cerr << "Error: " << error << " Description: " << description;
}

void Window::KeyCallback( GLFWwindow* window, int key, int scancode, int action, int mods )
{
    if( key == GLFW_KEY_ESCAPE && action == GLFW_PRESS )
        glfwSetWindowShouldClose( window, GL_TRUE );

    Window* win = reinterpret_cast<Window*>( glfwGetWindowUserPointer( window ) );
    Event event;
    event.mType = EventType::KeyboardKey;
    event.mKeyboard.Key = static_cast<KeyboardKey>( key );
    event.mKeyboard.Action = static_cast<KeyAction>( action );
    event.mKeyboard.Mods.SHIFT = mods & GLFW_MOD_SHIFT;
    event.mKeyboard.Mods.CONTROL = mods & GLFW_MOD_CONTROL;
    event.mKeyboard.Mods.ALT = mods & GLFW_MOD_ALT;
    event.mKeyboard.Mods.SUPER = mods & GLFW_MOD_SUPER;
    event.mKeyboard.Mods.CAPS_LOCK = mods & GLFW_MOD_CAPS_LOCK;
    event.mKeyboard.Mods.NUM_LOCK = mods & GLFW_MOD_NUM_LOCK;
    win->mEvents.push_back( event );
}

void Window::MouseButtonCallback( GLFWwindow* window, int key, int action, int mods )
{
    Window* win = reinterpret_cast<Window*>( glfwGetWindowUserPointer( window ) );
    Event event;
    event.mType = EventType::MouseKey; 
    event.mMouse.Key = static_cast<MouseKey>( key );
    event.mMouse.Action = static_cast<KeyAction>( action );
    event.mMouse.Mods.SHIFT = mods & GLFW_MOD_SHIFT;
    event.mMouse.Mods.CONTROL = mods & GLFW_MOD_CONTROL;
    event.mMouse.Mods.ALT = mods & GLFW_MOD_ALT;
    event.mMouse.Mods.SUPER = mods & GLFW_MOD_SUPER;
    event.mMouse.Mods.CAPS_LOCK = mods & GLFW_MOD_CAPS_LOCK;
    event.mMouse.Mods.NUM_LOCK = mods & GLFW_MOD_NUM_LOCK;
    win->mEvents.push_back( event );
}

void Window::MouseCallback( GLFWwindow* window, double xpos, double ypos )
{
    Window* win = reinterpret_cast<Window*>( glfwGetWindowUserPointer( window ) );
    auto mouse_pos = glm::vec2( xpos, ypos );
    win->mMouseInput.mMouseOffset = mouse_pos - win->mMouseInput.mMousePos;
    win->mMouseInput.mMousePos = mouse_pos;
    Event event;
    event.mType = EventType::MouseMove;
    event.mMouse.Pos = win->mMouseInput.mMousePos;
    event.mMouse.Offset = win->mMouseInput.mMouseOffset;
    win->mEvents.push_back( event );
}

void Window::ScrollCallback( GLFWwindow* window, double xoffset, double yoffset )
{
    Window* win = reinterpret_cast<Window*>( glfwGetWindowUserPointer( window ) );
    Event event;
    event.mType = EventType::MouseZoom;
    event.mMouse.ZoomOffset -= yoffset;
    win->mEvents.push_back( event );
}

void Window::WindowSizeCallback( GLFWwindow* window, int width, int height )
{
    Window* win = reinterpret_cast<Window*>( glfwGetWindowUserPointer( window ) );
    win->mWindowSize = WindowSize{ width, height };
    glfwSetWindowSize( win->mWindow, width, height );
}

void Window::FramebufferSizeCallback( GLFWwindow* window, int width, int height )
{
    Window* win = reinterpret_cast<Window*>( glfwGetWindowUserPointer( window ) );
    win->mWindowSize = WindowSize{ width, height };
    glViewport( 0, 0, width, height );
}


Window::Window( const std::string& name, WindowSize size )
    : mWindowSize( size )
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

    glfwSetWindowUserPointer( mWindow, this );
    glfwSetInputMode( mWindow, GLFW_LOCK_KEY_MODS, GLFW_TRUE );
    // set callback functions
    glfwSetErrorCallback( ErrorCallback );
    glfwSetKeyCallback( mWindow, KeyCallback );
    glfwSetMouseButtonCallback( mWindow, MouseButtonCallback );
    glfwSetCursorPosCallback( mWindow, MouseCallback );
    glfwSetScrollCallback( mWindow, ScrollCallback );
    glfwSetWindowSizeCallback( mWindow, WindowSizeCallback );
    glfwSetFramebufferSizeCallback( mWindow, FramebufferSizeCallback );

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

WindowSize Window::GetSize() const 
{
    return mWindowSize;
}

bool Window::ShouldClose() const
{
    return mShouldClose;
}


const std::vector<Event>& Window::PollEvents()
{
    mEvents.clear();
    glfwPollEvents();
    mShouldClose = glfwWindowShouldClose( mWindow );
    return mEvents;
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