#pragma once
#include <concepts>

namespace bubble
{

template <typename T>
int Sign( T num ) requires std::is_signed_v<T>
{
    return ( num > T( 0 ) ) - ( num < T( 0 ) );
}

}