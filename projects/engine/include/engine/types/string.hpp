#pragma once
#include <string>
#include <string_view>

namespace bubble
{
using namespace std::string_literals;
using namespace std::string_view_literals;

using string = std::string;
using string_view = std::string_view;


template <typename StrType>
StrType Slice( const StrType& str, size_t from, size_t to )
{
    auto len = std::min( to - from, str.size() - from );
    return str.substr( from, len );
}

struct string_hash
{
    using is_transparent = void;
    size_t operator()( const char* txt ) const { return std::hash<string_view>{}( txt ); }
    size_t operator()( string_view txt ) const { return std::hash<string_view>{}( txt ); }
    size_t operator()( const string& txt ) const { return std::hash<string>{}( txt ); }
};

}
