#pragma once
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <stdexcept>
#include <iostream>

#include "imexp.hpp"
#include "event.hpp"

namespace bubble
{

struct WindowSize
{
    int mWidth = 0;
    int mHeight = 0;
};

class BUBBLE_ENGINE_DLL_EXPORT Window
{
public:
    Window( const std::string& name, WindowSize size );
    ~Window();

    WindowSize GetSize();

    void PollEvents();
    void OnUpdate();
    bool ShouldClose();

    void SetVSync( bool vsync );

private:
    GLFWwindow* mWindow;
    WindowSize mWindowSize;
    bool mShouldClose = false;

};

}