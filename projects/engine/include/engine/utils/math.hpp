#pragma once
#include <concepts>

namespace bubble
{

template <typename T>
i32 Sign( T num ) requires std::is_signed_v<T>
{
    return ( num > T( 0 ) ) - ( num < T( 0 ) );
}

}