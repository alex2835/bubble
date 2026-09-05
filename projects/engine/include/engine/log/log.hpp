#pragma once
#include <iostream>
#include <format>

namespace bubble
{
template <typename ...Args>
void LogError( std::string_view format,  const Args& ...args )
{
    if constexpr ( sizeof...( Args ) == 0 )
        std::cout << "[Error] " << format << std::endl;
    else
        std::cout << "[Error] " << std::vformat(format, std::make_format_args(args...)) << std::endl;
}

template <typename ...Args>
void LogWarning( std::string_view format, const Args& ...args )
{
    if constexpr ( sizeof...( Args ) == 0 )
        std::cout << "[Warning] " << format << std::endl;
    else
        std::cout << "[Warning] " << std::vformat( format, std::make_format_args( args... ) ) << std::endl;
}

template <typename ...Args>
void LogInfo( std::string_view format, const Args& ...args )
{
    if constexpr ( sizeof...( Args ) == 0 )
        std::cout << "[Info] " << format << std::endl;
    else
        std::cout << "[Info] " << std::vformat(format, std::make_format_args(args...)) << std::endl;
}

}