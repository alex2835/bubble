#pragma once
#include <map>
#include <unordered_map>
#include "engine/types/string.hpp"

namespace bubble
{
template <typename K, typename V>
using map = std::map<K, V>;

template <typename K, typename V>
using hash_map = std::unordered_map<K, V>;

template <typename V>
using str_hash_map = std::unordered_map<string, V, string_hash, std::equal_to<>>;

}

