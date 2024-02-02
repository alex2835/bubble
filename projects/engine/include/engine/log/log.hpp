#pragma once
#include <iostream>
#include <format>

namespace bubble
{
template <typename ...Args>
void LogError( std::string_view format,  const Args& ...args )
{
    std::cout << "[Error] " << std::vformat(format, std::make_format_args(args...)) << "\n";
}

template <typename ...Args>
void LogWarning( std::string_view format, const Args& ...args )
{
    std::cout << "[Warning] " << std::vformat( format, std::make_format_args( args... ) ) << "\n";
}

template <typename ...Args>
void LogInfo( std::string_view format, const Args& ...args )
{
    std::cout << "[Info] " << std::vformat(format, std::make_format_args(args...)) << "\n";
}

}