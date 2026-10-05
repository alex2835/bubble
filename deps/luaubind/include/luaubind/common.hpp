#pragma once
#include <array>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

// The few names luaubind writes everywhere, from the standard library only:
// the library depends on Luau and nothing else.
namespace luaubind
{
using namespace std::string_literals;
using namespace std::string_view_literals;

using std::string;
using std::string_view;

template <typename T>
using vector = std::vector<T>;

template <typename T, size_t Size>
using array = std::array<T, Size>;

template <typename T>
using opt = std::optional<T>;

template <typename T, typename E>
using expected = std::expected<T, E>;

using u8 = std::uint8_t;
using i16 = std::int16_t;
using u32 = std::uint32_t;
using f32 = float;
using f64 = double;

// Keyed by string, looked up by string_view without a copy.
struct StringHash
{
    using is_transparent = void;
    size_t operator()( string_view text ) const { return std::hash<string_view>{}( text ); }
};

template <typename V>
using StringMap = std::unordered_map<string, V, StringHash, std::equal_to<>>;
}
