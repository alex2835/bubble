#pragma once
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
#include <stdexcept>
#include <iostream>
#include <vector>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_glfw.h>
#include <ImGuizmo.h>
//#include <ImSequencer.h>
//#include <ImZoomSlider.h>
//#include <ImCurveEdit.h>
//#include <GraphEditor.h>
#include "engine/window/event.hpp"
#include "engine/window/input.hpp"
#include "engine/utils/types.hpp"

namespace bubble
{
struct WindowSize
{
    u32 mWidth = 0;
    u32 mHeight = 0;
};

class Window
{
public:
    Window( const string& name, WindowSize size );
    ~Window();

    WindowSize Size() const;
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
    WindowSize mWindowSize;
    bool mShouldClose = false;
    vector<Event> mEvents;
    WindowInput mWindowInput;
};

}