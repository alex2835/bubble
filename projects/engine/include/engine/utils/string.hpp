
#include "types.hpp"

namespace bubble
{
template <typename StrType>
StrType Slice( const StrType& str, size_t from, size_t to )
{
    auto len = std::min( to - from, str.size() - from );
    return str.substr( from, len );
}

}
