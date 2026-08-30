
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
namespace
{
// A position saved on a monitor that is no longer attached would bring the
// window back completely off screen with no way to grab it. Keep at least a
// corner of it, big enough to be draggable, over some connected monitor.
ivec2 ClampToVisibleArea( ivec2 position, uvec2 size )
{
#if !defined(__EMSCRIPTEN__)
    constexpr i32 MIN_VISIBLE = 64;

    i32 monitorCount = 0;
    GLFWmonitor** monitors = glfwGetMonitors( &monitorCount );
    for ( i32 i = 0; i < monitorCount; i++ )
    {
        i32 areaX = 0, areaY = 0, areaWidth = 0, areaHeight = 0;
        glfwGetMonitorWorkarea( monitors[i], &areaX, &areaY, &areaWidth, &areaHeight );

        const i32 visibleX = std::min( position.x + (i32)size.x, areaX + areaWidth ) -
                             std::max( position.x, areaX );
        const i32 visibleY = std::min( position.y + (i32)size.y, areaY + areaHeight ) -
                             std::max( position.y, areaY );
        if ( visibleX >= MIN_VISIBLE and visibleY >= MIN_VISIBLE )
            return position;
    }

    GLFWmonitor* primary = glfwGetPrimaryMonitor();
    if ( not primary )
        return position;

    // Nothing overlaps, fall back to the middle of the primary monitor.
    i32 areaX = 0, areaY = 0, areaWidth = 0, areaHeight = 0;
    glfwGetMonitorWorkarea( primary, &areaX, &areaY, &areaWidth, &areaHeight );
    return ivec2{ areaX + std::max( 0, ( areaWidth - (i32)size.x ) / 2 ),
                  areaY + std::max( 0, ( areaHeight - (i32)size.y ) / 2 ) };
#else
    return position;
#endif
}
}

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

void Window::WindowPosCallback( GLFWwindow* window, i32 xpos, i32 ypos )
{
    Window* win = reinterpret_cast<Window*>( glfwGetWindowUserPointer( window ) );
    win->mWindowPos = ivec2{ xpos, ypos };
    win->UpdateRestoredGeometry();
}

void Window::WindowSizeCallback( GLFWwindow* window, i32 width, i32 height )
{
    Window* win = reinterpret_cast<Window*>( glfwGetWindowUserPointer( window ) );
    win->mWindowSize = uvec2{ (u32)width, (u32)height };
    win->UpdateRestoredGeometry();
}

void Window::UpdateRestoredGeometry()
{
    // An iconified window reports a zero size and a garbage position, and a
    // maximized one reports a rectangle there is no point restoring to.
    if ( IsMaximized() or glfwGetWindowAttrib( mWindow, GLFW_ICONIFIED ) )
        return;
    if ( mWindowSize == uvec2( 0u ) )
        return;

    mRestoredPos = mWindowPos;
    mRestoredSize = mWindowSize;
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

    // Created hidden so the caller can restore a saved position before the
    // window ever shows up, otherwise it flashes at the default spot first.
    glfwWindowHint( GLFW_VISIBLE, GLFW_FALSE );

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

    i32 windowPosX = 0, windowPosY = 0;
    glfwGetWindowPos( mWindow, &windowPosX, &windowPosY );
    mWindowPos = ivec2{ windowPosX, windowPosY };
    mRestoredPos = mWindowPos;
    mRestoredSize = mWindowSize;

    glfwSetWindowUserPointer( mWindow, this );
    glfwSetInputMode( mWindow, GLFW_LOCK_KEY_MODS, GLFW_TRUE );
    // set callback functions
    glfwSetKeyCallback( mWindow, KeyCallback );
    glfwSetMouseButtonCallback( mWindow, MouseButtonCallback );
    glfwSetCursorPosCallback( mWindow, MouseCallback );
    glfwSetScrollCallback( mWindow, ScrollCallback );
    glfwSetWindowPosCallback( mWindow, WindowPosCallback );
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

void Window::Show()
{
    glfwShowWindow( mWindow );
}

void Window::SetSize( uvec2 size )
{
    if ( size == uvec2( 0u ) )
        return;
    glfwSetWindowSize( mWindow, (i32)size.x, (i32)size.y );
}

ivec2 Window::Position() const
{
    return mWindowPos;
}

void Window::SetPosition( ivec2 position )
{
    i32 width = 0, height = 0;
    glfwGetWindowSize( mWindow, &width, &height );

    const auto visible = ClampToVisibleArea( position, uvec2{ (u32)width, (u32)height } );
    glfwSetWindowPos( mWindow, visible.x, visible.y );
}

ivec2 Window::RestoredPosition() const
{
    return mRestoredPos;
}

uvec2 Window::RestoredSize() const
{
    return mRestoredSize;
}

bool Window::IsMaximized() const
{
    return glfwGetWindowAttrib( mWindow, GLFW_MAXIMIZED ) == GLFW_TRUE;
}

void Window::SetMaximized( bool maximized )
{
    if ( maximized )
        glfwMaximizeWindow( mWindow );
    else
        glfwRestoreWindow( mWindow );
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

void Window::SetCursorMode( CursorMode mode )
{
    if ( mCursorMode == mode )
        return;

    mCursorMode = mode;

    i32 option = GLFW_CURSOR_NORMAL;
    switch ( mode )
    {
        case CursorMode::Normal: option = GLFW_CURSOR_NORMAL; break;
        case CursorMode::Hidden: option = GLFW_CURSOR_HIDDEN; break;
        case CursorMode::Locked: option = GLFW_CURSOR_DISABLED; break;
    }
    glfwSetInputMode( mWindow, GLFW_CURSOR, option );

#if !defined(__EMSCRIPTEN__)
    // Raw motion skips the desktop pointer acceleration curve, which is what a
    // camera wants and what a pointer never wants. Only valid while captured.
    if ( glfwRawMouseMotionSupported() )
        glfwSetInputMode( mWindow, GLFW_RAW_MOUSE_MOTION,
                          mode == CursorMode::Locked ? GLFW_TRUE : GLFW_FALSE );
#endif

    // GLFW moves the cursor when entering and leaving the captured mode. Adopt
    // the new position instead of reporting that jump as a mouse movement.
    SyncCursorPos();
}

CursorMode Window::GetCursorMode() const
{
    return mCursorMode;
}

void Window::LockCursor( bool lock )
{
    SetCursorMode( lock ? CursorMode::Locked : CursorMode::Normal );
}

void Window::HideCursor( bool hide )
{
    SetCursorMode( hide ? CursorMode::Hidden : CursorMode::Normal );
}

vec2 Window::GetCursorPos() const
{
    return mWindowInput.mMouseInput.mMousePos;
}

void Window::SetCursorPos( vec2 position )
{
    // Same y flip as the cursor position callback, the input frame has its
    // origin at the bottom left while GLFW puts it at the top left.
    glfwSetCursorPos( mWindow, position.x, mWindowSize.y - position.y );
    SyncCursorPos();
}

void Window::CenterCursor()
{
    SetCursorPos( vec2( mWindowSize ) * 0.5f );
}

void Window::SyncCursorPos()
{
    f64 xpos = 0.0, ypos = 0.0;
    glfwGetCursorPos( mWindow, &xpos, &ypos );
    mWindowInput.mMouseInput.mMousePos = vec2( xpos, mWindowSize.y - ypos );
    mWindowInput.mMouseInput.mMouseOffset = vec2( 0.0f );
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