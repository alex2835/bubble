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
#include "engine/utils/imexp.hpp"
#include "engine/window/event.hpp"
#include "engine/window/input.hpp"

namespace bubble
{
struct WindowSize
{
    uint32_t mWidth = 0;
    uint32_t mHeight = 0;
};

 
class BUBBLE_ENGINE_EXPORT Window
{
public:
    Window( const std::string& name, WindowSize size );
    ~Window();

    WindowSize GetSize() const;
    bool ShouldClose() const;

    const std::vector<Event>& PollEvents();
    void OnUpdate();

    void SetVSync( bool vsync );

    GLFWwindow* GetHandle() const;
    const char* GetGLSLVersion() const;

    ImGuiContext* GetImGuiContext();
    void ImGuiBegin();
    void ImGuiEnd();
private:
    Event CreateEvent() const;
    void FillKeyboardEvents();
    void FillMouseEvents();
    static void ErrorCallback( int error, const char* description );
    static void KeyCallback( GLFWwindow* window, int key, int scancode, int action, int mods );
    static void MouseButtonCallback( GLFWwindow* window, int key, int action, int mods );
    static void MouseCallback( GLFWwindow* window, double xpos, double ypos );
    static void ScrollCallback( GLFWwindow* window, double xoffset, double yoffset );
    static void WindowSizeCallback( GLFWwindow* window, int width, int height );
    static void FramebufferSizeCallback( GLFWwindow* window, int width, int height );

    GLFWwindow* mWindow;
    ImGuiContext* mImGuiContext;
    const char* mGLSLVersion;
    WindowSize mWindowSize;
    bool mShouldClose = false;
    MouseInput mMouseInput;
    KeyboardInput mKeyboardInput;
    std::vector<Event> mEvents;
};

}