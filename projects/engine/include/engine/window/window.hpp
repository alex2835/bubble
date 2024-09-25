#pragma once
#include "engine/types/number.hpp"
#include "engine/types/string.hpp"
#include "engine/types/array.hpp"
#include "engine/window/event.hpp"

struct GLFWwindow;
struct ImGuiContext;

namespace bubble
{
class Window
{
public:
    Window( const string& name, uvec2 size );
    ~Window();

    uvec2 Size() const;
    bool ShouldClose() const;

    const vector<Event>& PollEvents();
    void OnUpdate();

    bool IsKeyPressed( KeyboardKey key );
    bool IsKeyPressed( MouseKey key );

    void LockCursor( bool lock );
    void SetVSync( bool vsync );

    WindowInput& GetWindowInput();

    GLFWwindow* GetHandle() const;
    const char* GetGLSLVersion() const;

    void BindWindowFramebuffer();
    ImGuiContext* GetImGuiContext();
    void ImGuiBegin();
    void ImGuiEnd();
private:
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

    GLFWwindow* mWindow;
    ImGuiContext* mImGuiContext;
    const char* mGLSLVersion;
    ivec2 mWindowSize;
    bool mShouldClose = false;
    vector<Event> mEvents;
    WindowInput mWindowInput;
};

}