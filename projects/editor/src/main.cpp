#include <iostream>
#include "editor_application.hpp"

int main( void )
{
    try
    {
        bubble::BubbleEditor editorApplication;
        editorApplication.Run();
    }
    catch ( const std::exception& e )
    { 
        std::cout << e.what() << std::endl;
        return 1;
    }
    return 0;
}

