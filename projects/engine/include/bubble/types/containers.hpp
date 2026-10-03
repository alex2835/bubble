#pragma once
#include "bubble/types/string.hpp"
#include <array>
#include <map>
#include <set>
#include <span>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace bubble
{
template <typename T>
using vector = std::vector<T>;

template <typename T, size_t Size>
using array = std::array<T, Size>;

template <typename T>
using span = std::span<T>;

template <typename K, typename V>
using map = std::map<K, V>;

template <typename T>
using set = std::set<T>;

template <typename K, typename V>
using hash_map = std::unordered_map<K, V>;

template <typename T>
using hash_set = std::unordered_set<T>;

// Keyed by string, looked up by string_view without a copy.
template <typename V>
using str_hash_map = std::unordered_map<string, V, string_hash, std::equal_to<>>;
using str_hash_set = std::unordered_set<string, string_hash, std::equal_to<>>;
}
