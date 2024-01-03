#include <iostream>
#include "app/editor_app.hpp"

int main()
{
    try
    {
        bubble::BubbleEditor application;
        application.Run();
    }
    catch ( const std::exception& e )
    {
        std::cout << e.what() << std::endl;
        return 1;
    }
    return 0;
}

