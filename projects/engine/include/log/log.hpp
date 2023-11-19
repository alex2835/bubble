#pragma once
#include <iostream>
#include <format>

namespace bubble
{
template <typename ...Args>
void LogError( std::string_view format,  const Args& ...args )
{
    std::cout << std::vformat( format, std::make_format_args( args... ) ) << "\n";
}

}