#include <print>
#include <argagg/argagg.hpp>
#include "editor_application.hpp"

int main( int argc, char** argv )
{
    try
    {
        argagg::parser argparser
        {
            {
              {
                "help", {"-h", "--help"},
                "Print help and exit", 0
              },
              {
                "project", {"-p", "--project"},
                "Path to project file", 1
              }
            }
        };
        auto args = argparser.parse( argc, argv );

        if ( args["help"] )
        {
            std::cout << argparser;
            return 0;
        }

        bubble::BubbleEditor editorApplication;
        if ( args["project"] )
            editorApplication.OpenProject( args["project"].as<std::string>() );
        editorApplication.Run();
    }
    catch ( const std::exception& e )
    { 
        std::cout << e.what() << std::endl;
        return 1;
    }
    return 0;
}

