#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <string>

#include "window/window.hpp"
#include "serialization/event_serialization.hpp"

using namespace bubble;

int main()
{
    Window window( "Test", WindowSize{ 600, 400 } );
    while( !window.ShouldClose() )
    {
        auto events = window.PollEvents();
        for( auto event : events )
            std::cout << ToString( event ) << "\n\n";
    }
    return 0;
}