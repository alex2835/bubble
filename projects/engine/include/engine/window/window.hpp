#pragma once
#include <imgui.h>

#include "engine/types/number.hpp"
#include "engine/types/string.hpp"
#include "engine/types/array.hpp"
#include "engine/types/pointer.hpp"
#include "engine/window/event.hpp"

struct GLFWwindow;

namespace bubble
{
class GpuContext;
constexpr string_view INI_FILE_PATH = "./resources/imgui/imgui.ini"sv;
// Relative to the executable directory, see filesystem::resolveNearExecutable.
constexpr string_view DEFAULT_UI_FONT_PATH = "resources/fonts/Roboto-Medium.ttf"sv;
constexpr f32 DEFAULT_UI_FONT_SIZE = 16.0f;
constexpr f32 MIN_UI_SCALE = 0.5f;
constexpr f32 MAX_UI_SCALE = 3.0f;

enum class CursorMode
{
    // Visible, free to move anywhere and to leave the window.
    Normal,
    // Invisible, but still a real pointer that can leave the window.
    Hidden,
    // Invisible and captured, movement is reported as an unbounded virtual
    // position. This is the gameplay mode, a mouse look camera needs it.
    Locked
};

class Window
{
public:
    Window( const string& name, uvec2 size );
    ~Window();

    // The window is created hidden so saved geometry can be restored before it
    // ever appears on screen. Nothing is visible until Show() is called.
    void Show();

    // Window size in screen coordinates (what GLFW reports for the window).
    uvec2 Size() const;
    // Window size in pixels. Differs from Size() on DPI scaled displays,
    // this is what glViewport and any pixel math must use.
    uvec2 FramebufferSize() const;
    void SetSize( uvec2 size );

    // Position of the top left corner in screen coordinates.
    ivec2 Position() const;
    // Clamps onto a connected monitor, a position saved on a display that is no
    // longer attached would otherwise bring the window back out of reach.
    // Uses the current size, so restore the size before the position.
    void SetPosition( ivec2 position );

    // Geometry the window returns to when it is not maximized. This is what to
    // persist, saving the maximized rectangle would leave nothing sane to
    // restore to once the user unmaximizes.
    ivec2 RestoredPosition() const;
    uvec2 RestoredSize() const;

    bool IsMaximized() const;
    void SetMaximized( bool maximized );

    bool ShouldClose() const;

    const vector<Event>& PollEvents();
    void OnUpdate();

    bool IsKeyPressed( KeyboardKey key );
    bool IsKeyPressed( MouseKey key );

    void SetCursorMode( CursorMode mode );
    CursorMode GetCursorMode() const;
    // Convenience wrappers around SetCursorMode.
    void LockCursor( bool lock );
    void HideCursor( bool hide );

    // Cursor position in the same frame as WindowInput::MousePos, origin at the
    // bottom left corner. Setting it also resyncs the tracked mouse position so
    // the jump is not reported as a mouse movement offset.
    vec2 GetCursorPos() const;
    void SetCursorPos( vec2 position );
    void CenterCursor();

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

    ImGuiContext* GetImGuiContext();
    void ImGuiBegin();
    void ImGuiEnd();

private:
    void ApplyUIScale();
    void ReloadUIFont();
    // Adopts whatever the OS thinks the cursor position is as the current one,
    // dropping the accumulated offset.
    void SyncCursorPos();
    void UpdateRestoredGeometry();
    Event CreateEvent() const;
    void FillKeyboardEvents();
    void FillMouseEvents();
    static void ErrorCallback( i32 error, const char* description );
    static void KeyCallback( GLFWwindow* window, i32 key, i32 scancode, i32 action, i32 mods );
    static void MouseButtonCallback( GLFWwindow* window, i32 key, i32 action, i32 mods );
    static void MouseCallback( GLFWwindow* window, f64 xpos, f64 ypos );
    static void ScrollCallback( GLFWwindow* window, f64 xoffset, f64 yoffset );
    static void WindowPosCallback( GLFWwindow* window, i32 xpos, i32 ypos );
    static void WindowSizeCallback( GLFWwindow* window, i32 width, i32 height );
    static void FramebufferSizeCallback( GLFWwindow* window, i32 width, i32 height );

private:
    GLFWwindow* mWindow;
    // Created first and destroyed last, so it outlives every GPU resource.
    Scope<GpuContext> mGpuContext;
    ImGuiContext* mImGuiContext;
    uvec2 mWindowSize = uvec2( 0u );
    uvec2 mFramebufferSize = uvec2( 0u );
    ivec2 mWindowPos = ivec2( 0 );
    // Last geometry seen while the window was neither maximized nor iconified.
    ivec2 mRestoredPos = ivec2( 0 );
    uvec2 mRestoredSize = uvec2( 0u );
    CursorMode mCursorMode = CursorMode::Normal;
    bool mShouldClose = false;
    // Whether ImGuiEnd managed to acquire a surface texture this frame. Present
    // must not be called otherwise.
    bool mFramePresentable = false;

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