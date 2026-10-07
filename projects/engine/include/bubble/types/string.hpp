#pragma once
#include "bubble/types/utility.hpp"
#include <charconv>
#include <string>
#include <string_view>

namespace bubble
{
using namespace std::string_literals;
using namespace std::string_view_literals;

using string = std::string;
using string_view = std::string_view;
using std::u8string;
using std::u8string_view;

using std::to_string;

// The whole of `text` as an integer, or nothing.
template <typename Int = int>
opt<Int> TryParse( string_view text )
{
    Int value = 0;
    const auto [end, error] = std::from_chars( text.data(), text.data() + text.size(), value );
    if ( error != std::errc() or end != text.data() + text.size() )
        return std::nullopt;
    return value;
}

// Lets hash containers keyed by string look up by string_view without a copy.
struct string_hash
{
    using is_transparent = void;
    size_t operator()( string_view text ) const { return std::hash<string_view>{}( text ); }
};
}
