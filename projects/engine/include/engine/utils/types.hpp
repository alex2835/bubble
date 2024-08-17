#pragma once
#include "engine/utils/pointers.hpp"
#include <glm/glm.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm-aabb/AABB.hpp>
#include <nlohmann/json_fwd.hpp>
#include <string>
#include <string_view>
#include <vector>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <array>
#include <map>
#include <optional>
#include <exception>
#include <tuple>
#include <format>
#include <ranges>
#include <future>
#include <span>

namespace bubble
{
using namespace glm;
using namespace CPM_GLM_AABB_NS;
using namespace nlohmann;
using namespace std::string_literals;
using namespace std::string_view_literals;
namespace ranges = std::ranges;
namespace views = std::ranges::views;

using i8 = std::int8_t;
using u8 = std::uint8_t;
using i16 = std::int16_t;
using u16 = std::uint16_t;
using i32 = std::int32_t;
using u32 = std::uint32_t;
using i64 = std::int64_t;
using u64 = std::uint64_t;
using f32 = std::float_t;
using f64 = std::double_t;

using string = std::string;
using string_view = std::string_view;
template <typename F, typename S>
using pair = std::pair<F, S>;
template <typename T>
using vector = std::vector<T>;
template <typename K, typename V>
using map = std::map<K, V>;
template <typename T>
using set = std::set<T>;
template <typename K, typename V>
using hash_map = std::unordered_map<K, V>;
template <typename T>
using hash_set = std::unordered_set<T>;
template <typename T, u64 SIZE>
using array = std::array<T, SIZE>;
template <typename T>
using opt = std::optional<T>;
template <typename T, typename S>
using pair = std::pair<T, S>;
template <typename T>
using future = std::future<T>;
template <typename T>
using span = std::span<T>;

struct string_hash
{
    using is_transparent = void;
    size_t operator()( const char* txt ) const { return std::hash<string_view>{}( txt ); }
    size_t operator()( string_view txt ) const { return std::hash<string_view>{}( txt ); }
    size_t operator()( const string& txt ) const { return std::hash<string>{}( txt ); }
};
template <typename V>
using str_hash_map = std::unordered_map<string, V, string_hash, std::equal_to<>>;
using str_hash_set = std::unordered_set<string, string_hash, std::equal_to<>>;

}

