#pragma once
#include "bubble/types/string.hpp"
#include <array>
#include <deque>
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

template <typename T>
using deque = std::deque<T>;

template <typename T, size_t Size>
using array = std::array<T, Size>;

template <typename T>
using span = std::span<T>;

template <typename K, typename V>
using map = std::map<K, V>;

template <typename T>
using set = std::set<T>;

// The hash of a key; a string key gets one that also takes string_view and
// const char*, so a lookup does not copy.
template <typename K>
struct hmap_hash
{
    using type = std::hash<K>;
};

template <>
struct hmap_hash<string>
{
    using type = string_hash;
};

// std::equal_to<> compares with ==; lookup by another type works only where
// the hash is transparent too, that is for string keys.
template <typename K, typename V>
using hmap = std::unordered_map<K, V, typename hmap_hash<K>::type, std::equal_to<>>;

template <typename T>
using hset = std::unordered_set<T, typename hmap_hash<T>::type, std::equal_to<>>;

using std::erase_if;
}
