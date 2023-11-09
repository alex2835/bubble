#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <string>

#include "window.hpp"

using namespace bubble;

int main()
{
    Window window( "Test", WindowSize{ 600, 400 } );
    while( !window.ShouldClose() )
        window.PollEvents();
    return 0;
}