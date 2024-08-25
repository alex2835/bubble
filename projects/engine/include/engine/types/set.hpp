#pragma once
#include <set>
#include <unordered_set>
#include "engine/types/string.hpp"

namespace bubble
{
template <typename T>
using set = std::set<T>;

template <typename T>
using hash_set = std::unordered_set<T>;

using str_hash_set = std::unordered_set<string, string_hash, std::equal_to<>>;

}