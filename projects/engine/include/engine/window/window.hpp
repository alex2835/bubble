#pragma once
#include <imgui.h>

#include "engine/types/number.hpp"
#include "engine/types/string.hpp"
#include "engine/types/array.hpp"
#include "engine/window/event.hpp"

struct GLFWwindow;

namespace bubble
{
constexpr string_view INI_FILE_PATH = "./resources/imgui/imgui.ini"sv;
constexpr string_view DEFAULT_UI_FONT_PATH = "./resources/fonts/Roboto-Medium.ttf"sv;
constexpr f32 DEFAULT_UI_FONT_SIZE = 16.0f;
constexpr f32 MIN_UI_SCALE = 0.5f;
constexpr f32 MAX_UI_SCALE = 3.0f;

class Window
{
public:
    Window( const string& name, uvec2 size );
    ~Window();

    // Window size in screen coordinates (what GLFW reports for the window).
    uvec2 Size() const;
    // Window size in pixels. Differs from Size() on DPI scaled displays,
    // this is what glViewport and any pixel math must use.
    uvec2 FramebufferSize() const;
    bool ShouldClose() const;

    const vector<Event>& PollEvents();
    void OnUpdate();

    bool IsKeyPressed( KeyboardKey key );
    bool IsKeyPressed( MouseKey key );

    void LockCursor( bool lock );
    void SetVSync( bool vsync );

    // User interface scaling.
    // Scales both the font and all style metrics (padding, rounding, scrollbars).
    // Independent of monitor DPI, meant to be exposed to the user, which is the
    // only thing that helps on large low DPI displays where GetDPIScale() is 1.
    void SetUIScale( f32 scale );
    f32 GetUIScale() const;

    // Unscaled font height in pixels. Final height is
    // GetUIFontSize() * GetUIScale() * GetDPIScale().
    void SetUIFontSize( f32 sizePixels );
    f32 GetUIFontSize() const;

    // Swaps the interface font. Applied at the start of the next frame, so it
    // is safe to call from inside imgui drawing code. Pass an empty path to
    // fall back to the built in font.
    void SetUIFont( string_view fontPath );
    const string& GetUIFont() const;

    // Content scale reported by the monitor the window is on.
    f32 GetDPIScale() const;

    WindowInput& GetWindowInput();

    GLFWwindow* GetHandle() const;
    const char* GetGLSLVersion() const;

    void BindWindowFramebuffer();
    ImGuiContext* GetImGuiContext();
    void ImGuiBegin();
    void ImGuiEnd();

private:
    void ApplyUIScale();
    void ReloadUIFont();
    Event CreateEvent() const;
    void FillKeyboardEvents();
    void FillMouseEvents();
    static void ErrorCallback( i32 error, const char* description );
    static void KeyCallback( GLFWwindow* window, i32 key, i32 scancode, i32 action, i32 mods );
    static void MouseButtonCallback( GLFWwindow* window, i32 key, i32 action, i32 mods );
    static void MouseCallback( GLFWwindow* window, f64 xpos, f64 ypos );
    static void ScrollCallback( GLFWwindow* window, f64 xoffset, f64 yoffset );
    static void WindowSizeCallback( GLFWwindow* window, i32 width, i32 height );
    static void FramebufferSizeCallback( GLFWwindow* window, i32 width, i32 height );

private:
    GLFWwindow* mWindow;
    ImGuiContext* mImGuiContext;
    const char* mGLSLVersion;
    uvec2 mWindowSize = uvec2( 0u );
    uvec2 mFramebufferSize = uvec2( 0u );
    bool mShouldClose = false;

    // Style metrics as created by the theme, before any scaling is applied.
    // ImGuiStyle::ScaleAllSizes is cumulative so rescaling always restarts from these.
    ImGuiStyle mBaseStyle;
    f32 mUIScale = 1.0f;
    f32 mUIFontSize = DEFAULT_UI_FONT_SIZE;
    string mUIFontPath;
    bool mUIFontDirty = false;
    vector<Event> mEvents;
    WindowInput mWindowInput;
};

}