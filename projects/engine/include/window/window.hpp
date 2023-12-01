#pragma once
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <stdexcept>
#include <iostream>
#include <vector>

#include "utils/imexp.hpp"
#include "window/event.hpp"
#include "window/input.hpp"

namespace bubble
{

struct WindowSize
{
    int mWidth = 0;
    int mHeight = 0;
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

private:
    static void ErrorCallback( int error, const char* description );
    static void KeyCallback( GLFWwindow* window, int key, int scancode, int action, int mods );
    static void MouseButtonCallback( GLFWwindow* window, int key, int action, int mods );
    static void MouseCallback( GLFWwindow* window, double xpos, double ypos );
    static void ScrollCallback( GLFWwindow* window, double xoffset, double yoffset );
    static void WindowSizeCallback( GLFWwindow* window, int width, int height );
    static void FramebufferSizeCallback( GLFWwindow* window, int width, int height );

    GLFWwindow* mWindow;
    const char* mGLSLVersion;
    WindowSize mWindowSize;
    bool mShouldClose = false;
    MouseInput mMouseInput;
    std::vector<Event> mEvents;
};

}