
#if defined(__EMSCRIPTEN__)
#   include <emscripten.h>
#   include <emscripten/html5.h>
#   define GL_GLEXT_PROTOTYPES
#   define EGL_EGLEXT_PROTOTYPES
#else
#   include <GL/glew.h>
#   include <GLFW/glfw3.h>
#   if defined(IMGUI_IMPL_OPENGL_ES2)
#       include <GLES2/gl2.h>
#   endif
#endif
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_glfw.h>

#include "engine/window/event.hpp"
#include "engine/window/input.hpp"
#include "engine/window/window.hpp"
#include "engine/types/number.hpp"
#include "engine/log/log.hpp"


namespace bubble
{
void Window::ErrorCallback( i32 error, const char* description )
{
    LogError( "Error: {} \nDescription: {}", error, description );
}

void Window::KeyCallback( GLFWwindow* window, i32 key, i32 scancode, i32 action, i32 mods )
{
    Window* win = reinterpret_cast<Window*>( glfwGetWindowUserPointer( window ) );

    //if ( key == GLFW_KEY_ESCAPE && action == GLFW_PRESS )
    //{
    //    glfwSetWindowShouldClose( window, GL_TRUE );
    //    Event event = win->CreateEvent();
    //    event.mType = EventType::ShouldClose;
    //    win->mEvents.push_back( event );
    //}
    win->mWindowInput.mKeyboardInput.mKeyState[key] = action;
    win->mWindowInput.mKeyboardInput.mKeyMods.SHIFT = mods & GLFW_MOD_SHIFT;
    win->mWindowInput.mKeyboardInput.mKeyMods.CONTROL = mods & GLFW_MOD_CONTROL;
    win->mWindowInput.mKeyboardInput.mKeyMods.ALT = mods & GLFW_MOD_ALT;
    win->mWindowInput.mKeyboardInput.mKeyMods.SUPER = mods & GLFW_MOD_SUPER;
    win->mWindowInput.mKeyboardInput.mKeyMods.CAPS_LOCK = mods & GLFW_MOD_CAPS_LOCK;
    win->mWindowInput.mKeyboardInput.mKeyMods.NUM_LOCK = mods & GLFW_MOD_NUM_LOCK;
}

void Window::MouseButtonCallback( GLFWwindow* window, i32 key, i32 action, i32 mods )
{
    Window* win = reinterpret_cast<Window*>( glfwGetWindowUserPointer( window ) );
    win->mWindowInput.mMouseInput.mKeyState[key] = action;
    win->mWindowInput.mMouseInput.mKeyMods.SHIFT = mods & GLFW_MOD_SHIFT;
    win->mWindowInput.mMouseInput.mKeyMods.CONTROL = mods & GLFW_MOD_CONTROL;
    win->mWindowInput.mMouseInput.mKeyMods.ALT = mods & GLFW_MOD_ALT;
    win->mWindowInput.mMouseInput.mKeyMods.SUPER = mods & GLFW_MOD_SUPER;
    win->mWindowInput.mMouseInput.mKeyMods.CAPS_LOCK = mods & GLFW_MOD_CAPS_LOCK;
    win->mWindowInput.mMouseInput.mKeyMods.NUM_LOCK = mods & GLFW_MOD_NUM_LOCK;
}

void Window::MouseCallback( GLFWwindow* window, f64 xpos, f64 ypos )
{
    Window* win = reinterpret_cast<Window*>( glfwGetWindowUserPointer( window ) );
    auto window_size = win->Size();
    auto mouse_pos = vec2( xpos, window_size.y - ypos );
    win->mWindowInput.mMouseInput.mMouseOffset = mouse_pos - win->mWindowInput.mMouseInput.mMousePos;
    win->mWindowInput.mMouseInput.mMousePos = mouse_pos;

    Event event = win->CreateEvent();
    event.mType = EventType::MouseMove;
    event.mMouse.Pos = win->mWindowInput.mMouseInput.mMousePos;
    event.mMouse.Offset = win->mWindowInput.mMouseInput.mMouseOffset;
    win->mEvents.push_back( event );
}

void Window::ScrollCallback( GLFWwindow* window, f64 xoffset, f64 yoffset )
{
    Window* win = reinterpret_cast<Window*>( glfwGetWindowUserPointer( window ) );
    Event event = win->CreateEvent();
    event.mType = EventType::MouseZoom;
    event.mMouse.ZoomOffset -= static_cast<f32>( yoffset );
    win->mEvents.push_back( event );
}

void Window::WindowSizeCallback( GLFWwindow* window, i32 width, i32 height )
{
    Window* win = reinterpret_cast<Window*>( glfwGetWindowUserPointer( window ) );
    win->mWindowSize = uvec2{ (u32)width, (u32)height };
    glfwSetWindowSize( win->mWindow, width, height );
}

void Window::FramebufferSizeCallback( GLFWwindow* window, i32 width, i32 height )
{
    Window* win = reinterpret_cast<Window*>( glfwGetWindowUserPointer( window ) );
    win->mWindowSize = uvec2{ (u32)width, (u32)height };
    glViewport( 0, 0, width, height );
}

void Window::FillKeyboardEvents()
{
    for ( u64 key = 0; key < MAX_KEYBOAR_KEYS_SIZE; key++ )
    {
        if ( mWindowInput.mKeyboardInput.mKeyState[key] == NO_STATE )
            continue;

        auto action = mWindowInput.mKeyboardInput.mKeyState[key];
        if ( mWindowInput.mKeyboardInput.mKeyState[key] == (i32)KeyAction::RELEASE )
            mWindowInput.mKeyboardInput.mKeyState[key] = NO_STATE;

        Event event = CreateEvent();
        event.mType = EventType::KeyboardKey;
        event.mKeyboard.Key = static_cast<KeyboardKey>( key );
        event.mKeyboard.Action = static_cast<KeyAction>( action );
        event.mKeyboard.Mods = mWindowInput.mKeyboardInput.mKeyMods;
        mEvents.push_back( event );
    }
}

void Window::FillMouseEvents()
{
    for ( u64 key = 0; key < MAX_MOUSE_KEYS_SIZE; key++ )
    {
        if ( mWindowInput.mMouseInput.mKeyState[key] == NO_STATE )
            continue;

        auto action = mWindowInput.mMouseInput.mKeyState[key];
        if ( mWindowInput.mMouseInput.mKeyState[key] == (i32)KeyAction::RELEASE )
            mWindowInput.mMouseInput.mKeyState[key] = NO_STATE;

        Event event= CreateEvent();
        event.mType = EventType::MouseKey;
        event.mMouse.Key = static_cast<MouseKey>( key );
        event.mMouse.Action = static_cast<KeyAction>( action );
        event.mMouse.Mods = mWindowInput.mMouseInput.mKeyMods;
        mEvents.push_back( event );
    }
}

bubble::Event Window::CreateEvent() const
{
    Event event;
    event.mKeyboardInput = &mWindowInput.mKeyboardInput;
    event.mMouseInput = &mWindowInput.mMouseInput;
    return event;
}



Window::Window( const string& name, uvec2 size )
    : mWindowSize( size )
{
    glfwSetErrorCallback( ErrorCallback );
    if( !glfwInit() )
        throw std::runtime_error( "GLFW init error" );

    // Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
    // GL ES 2.0 + GLSL 100
    mGLSLVersion = "#version 100";
    glfwWindowHint( GLFW_CONTEXT_VERSION_MAJOR, 2 );
    glfwWindowHint( GLFW_CONTEXT_VERSION_MINOR, 0 );
    glfwWindowHint( GLFW_CLIENT_API, GLFW_OPENGL_ES_API );
#elif defined(__APPLE__)
    // GL 3.2 + GLSL 150
    mGLSLVersion = "#version 150";
    glfwWindowHint( GLFW_CONTEXT_VERSION_MAJOR, 3 );
    glfwWindowHint( GLFW_CONTEXT_VERSION_MINOR, 2 );
    glfwWindowHint( GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE );  // 3.2+ only
    glfwWindowHint( GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE );            // Required on Mac
#else
    // GL 3.0 + GLSL 130
    mGLSLVersion = "#version 130";
    glfwWindowHint( GLFW_CONTEXT_VERSION_MAJOR, 3 );
    glfwWindowHint( GLFW_CONTEXT_VERSION_MINOR, 3 );
    glfwWindowHint( GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE );  // 3.2+ only
    //glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only
#endif

    mWindow = glfwCreateWindow( size.x, size.y, name.c_str(), NULL, NULL );
    if( !mWindow )
    {
        glfwTerminate();
        throw std::runtime_error( "GLFW window creation error" );
    }
    glfwMakeContextCurrent( mWindow );

    glfwSetWindowUserPointer( mWindow, this );
    glfwSetInputMode( mWindow, GLFW_LOCK_KEY_MODS, GLFW_TRUE );
    // set callback functions
    glfwSetKeyCallback( mWindow, KeyCallback );
    glfwSetMouseButtonCallback( mWindow, MouseButtonCallback );
    glfwSetCursorPosCallback( mWindow, MouseCallback );
    glfwSetScrollCallback( mWindow, ScrollCallback );
    glfwSetWindowSizeCallback( mWindow, WindowSizeCallback );
    glfwSetFramebufferSizeCallback( mWindow, FramebufferSizeCallback );

#if !defined(__EMSCRIPTEN__)
    i32 err = glewInit();
    if ( GLEW_OK != err )
    {
        std::cerr << "Error: " << glewGetErrorString( err ) << std::endl;
        glfwTerminate();
        throw std::runtime_error( "GLEW init error" );
    }
#endif

    // Set up ImGui
    IMGUI_CHECKVERSION();
    mImGuiContext = ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_DpiEnableScaleViewports;
    io.ConfigFlags |= ImGuiConfigFlags_DpiEnableScaleFonts;

    //LoadIniSettingsFromMemory()
    //io.IniFilename = nullptr;

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    if ( io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable )
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }
    ImGui_ImplGlfw_InitForOpenGL( mWindow, true );
    ImGui_ImplOpenGL3_Init( mGLSLVersion );
}

Window::~Window()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow( mWindow );
    glfwTerminate();
}

uvec2 Window::Size() const
{
    return mWindowSize;
}

bool Window::ShouldClose() const
{
    return mShouldClose;
}

const vector<Event>& Window::PollEvents()
{
    mEvents.clear();
    glfwPollEvents();
    FillKeyboardEvents();
    FillMouseEvents();
    mShouldClose = glfwWindowShouldClose( mWindow );
    return mEvents;
}

void Window::OnUpdate()
{
    mWindowInput.mMouseInput.OnUpdate();
    mWindowInput.mKeyboardInput.OnUpdate();
    glfwSwapBuffers( mWindow );
}

bool Window::IsKeyPressed( KeyboardKey key )
{
    return mWindowInput.mKeyboardInput.IsKeyPressed( key );
}

bool Window::IsKeyPressed( MouseKey key )
{
    return mWindowInput.mMouseInput.IsKeyPressed( key );
}

void Window::LockCursor( bool lock )
{
    auto option = lock ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL;
    glfwSetInputMode( mWindow, GLFW_CURSOR, option );
}

void Window::SetVSync( bool vsync )
{
    glfwSwapInterval( vsync );
}

WindowInput& Window::GetWindowInput()
{
    return mWindowInput;
}

GLFWwindow* Window::GetHandle() const
{
    return mWindow;
}

const char* Window::GetGLSLVersion() const
{
    return mGLSLVersion;
}

void Window::BindWindowFramebuffer()
{
    glBindFramebuffer( GL_FRAMEBUFFER, 0 );
    glViewport( 0, 0, mWindowSize.x, mWindowSize.y );
}

ImGuiContext* Window::GetImGuiContext()
{
    return mImGuiContext;
}

void Window::ImGuiBegin()
{
    BindWindowFramebuffer();
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    ImGui::DockSpaceOverViewport( ImGui::GetMainViewport() );
}

void Window::ImGuiEnd()
{
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData( ImGui::GetDrawData() );

    // Multi view ports
    //ImGuiIO& io = ImGui::GetIO();
    //if ( io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable )
    //{
    //    ImGui::UpdatePlatformWindows();
    //    ImGui::RenderPlatformWindowsDefault();
    //    glfwMakeContextCurrent( mWindow );
    //}
}


}