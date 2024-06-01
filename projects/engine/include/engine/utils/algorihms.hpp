#pragma once
#include "types.hpp"
#include <ranges>

namespace bubble
{
template <typename Container>
void RangeMoveBack( Container& container, Container& moveFrom )
{
    container.insert( container.begin(), 
                      std::move_iterator( moveFrom.begin() ),
                      std::move_iterator( moveFrom.end() ) );
}

}
