
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

#include <algorithm>

#include "engine/window/event.hpp"
#include "engine/window/input.hpp"
#include "engine/window/window.hpp"
#include "engine/types/number.hpp"
#include "engine/utils/filesystem.hpp"
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
    // Cursor position is in screen coordinates, so the y flip must use the
    // window size in screen coordinates, not the framebuffer size.
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
}

void Window::FramebufferSizeCallback( GLFWwindow* window, i32 width, i32 height )
{
    Window* win = reinterpret_cast<Window*>( glfwGetWindowUserPointer( window ) );
    win->mFramebufferSize = uvec2{ (u32)width, (u32)height };
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

    // Screen coordinates and pixels are not the same on DPI scaled displays,
    // query both instead of assuming the requested size is valid for either.
    i32 windowWidth = 0, windowHeight = 0;
    glfwGetWindowSize( mWindow, &windowWidth, &windowHeight );
    mWindowSize = uvec2{ (u32)windowWidth, (u32)windowHeight };

    i32 framebufferWidth = 0, framebufferHeight = 0;
    glfwGetFramebufferSize( mWindow, &framebufferWidth, &framebufferHeight );
    mFramebufferSize = uvec2{ (u32)framebufferWidth, (u32)framebufferHeight };

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
    // io.ConfigFlags |= ImGuiConfigFlags_DpiEnableScaleViewports;
    // io.ConfigFlags |= ImGuiConfigFlags_DpiEnableScaleFonts;

    //LoadIniSettingsFromMemory()
    io.IniFilename = INI_FILE_PATH.data();

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    if ( io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable )
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }
    // Keep the unscaled metrics around, ScaleAllSizes is cumulative so every
    // rescale has to restart from these instead of the current values.
    mBaseStyle = style;

    // Monitor DPI. Stays 1.0 on large low DPI displays (a 32" 1440p panel is
    // about 92 PPI), which is why SetUIScale exists as a separate user knob.
    style.FontScaleDpi = GetDPIScale();

    mUIFontPath = DEFAULT_UI_FONT_PATH;
    ReloadUIFont();
    ApplyUIScale();

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

uvec2 Window::FramebufferSize() const
{
    return mFramebufferSize;
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
    // Window is hidden so no need rendering
    if ( FramebufferSize() != uvec2( 0u ) )
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

void Window::SetUIScale( f32 scale )
{
    mUIScale = std::clamp( scale, MIN_UI_SCALE, MAX_UI_SCALE );
    ApplyUIScale();
}

f32 Window::GetUIScale() const
{
    return mUIScale;
}

void Window::SetUIFontSize( f32 sizePixels )
{
    mUIFontSize = std::clamp( sizePixels, 8.0f, 72.0f );
    ApplyUIScale();
}

f32 Window::GetUIFontSize() const
{
    return mUIFontSize;
}

void Window::SetUIFont( string_view fontPath )
{
    if ( mUIFontPath == fontPath )
        return;

    mUIFontPath = fontPath;
    // The atlas must not be touched between NewFrame and Render, so defer the
    // actual swap to the start of the next frame.
    mUIFontDirty = true;
}

const string& Window::GetUIFont() const
{
    return mUIFontPath;
}

f32 Window::GetDPIScale() const
{
    f32 xscale = 1.0f, yscale = 1.0f;
    glfwGetWindowContentScale( mWindow, &xscale, &yscale );
    return xscale > 0.0f ? xscale : 1.0f;
}

void Window::ApplyUIScale()
{
    ImGuiStyle& style = ImGui::GetStyle();

    // Restore the unscaled metrics, keeping whatever colors are currently set
    // so a theme change does not get reverted by a rescale.
    ImVec4 colors[ImGuiCol_COUNT];
    std::copy_n( style.Colors, ImGuiCol_COUNT, colors );
    style = mBaseStyle;
    std::copy_n( colors, ImGuiCol_COUNT, style.Colors );

    style.ScaleAllSizes( mUIScale );
    style.FontScaleMain = mUIScale;
    style.FontScaleDpi = GetDPIScale();

    // NewFrame resets FontSizeBase from its own copy, so a change made while a
    // frame is in flight has to go through _NextFrameFontSizeBase to survive.
    // This is what the imgui style editor does as well.
    style.FontSizeBase = mUIFontSize;
    style._NextFrameFontSizeBase = mUIFontSize;
}

void Window::ReloadUIFont()
{
    mUIFontDirty = false;

    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();

    if ( mUIFontPath.empty() )
    {
        io.Fonts->AddFontDefault();
        return;
    }

    // Relative paths are resolved against the executable, not the working
    // directory, so the editor finds its font whatever it was launched from.
    auto fontFile = filesystem::resolveNearExecutable( mUIFontPath );
    if ( not filesystem::exists( fontFile ) )
    {
        LogError( "UI font not found: {}, falling back to the built in font", fontFile.string() );
        mUIFontPath.clear();
        io.Fonts->AddFontDefault();
        return;
    }

    // Size 0 keeps the font dynamic, imgui 1.92 bakes it on demand at whatever
    // size style.FontSizeBase and the scale factors ask for.
    io.Fonts->AddFontFromFileTTF( fontFile.string().c_str(), 0.0f );
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
    glViewport( 0, 0, mFramebufferSize.x, mFramebufferSize.y );
}

ImGuiContext* Window::GetImGuiContext()
{
    return mImGuiContext;
}

void Window::ImGuiBegin()
{
    if ( mUIFontDirty )
        ReloadUIFont();

    BindWindowFramebuffer();
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    ImGui::DockSpaceOverViewport( 0, ImGui::GetMainViewport() );
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