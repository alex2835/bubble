#include "engine/pch/pch.hpp"
#include "editor_application/editor_application.hpp"
#include <argagg/argagg.hpp>
#include <print>
#include <sstream>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace
{
// With no terminal, something that has to be seen before the editor's own
// window is up - the usage, a failure to start - is shown in a message box.
void Tell( const std::string& text, bool error )
{
#ifdef _WIN32
    MessageBoxA( nullptr, text.c_str(), "Bubble", MB_OK | ( error ? MB_ICONERROR : MB_ICONINFORMATION ) );
#else
    ( error ? std::cerr : std::cout ) << text << std::endl;
#endif
}

// Next to the executable, so it is found without knowing where the editor
// was started from; truncated each run.
std::filesystem::path LogFilePath( const char* argv0 )
{
    std::error_code ec;
    const auto exe = std::filesystem::absolute( argv0, ec );
    return ( ec ? std::filesystem::current_path() : exe.parent_path() ) / "bubble_editor.log";
}
}


int main( int argc, char** argv )
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
          },
          {
            "script", {"-s", "--script"},
            "Editor Lua script to run after the project is opened", 1
          }
        }
    };

    bubble::LogToFile( LogFilePath( argv[0] ) );
    try
    {
        auto args = argparser.parse( argc, argv );
        if ( args["help"] )
        {
            std::ostringstream usage;
            usage << argparser;
            Tell( usage.str(), false );
            return 0;
        }

        bubble::BubbleEditor editorApplication;
        if ( args["project"] )
            editorApplication.OpenProject( args["project"].as<std::string>() );
        if ( args["script"] )
            editorApplication.RunScript( args["script"].as<std::string>() );
        editorApplication.Run();
    }
    catch ( const std::exception& e )
    {
        bubble::LogError( "{}", e.what() );
        Tell( e.what(), true );
        return 1;
    }
    return 0;
}
