#pragma once
#include <vector>
#include <array>
#include <span>
#include <ranges>

namespace bubble
{
namespace ranges = std::ranges;
namespace views = std::ranges::views;

template <typename T>
using vector = std::vector<T>;

template <typename T>
using span = std::span<T>;

template <typename T, size_t SIZE>
using array = std::array<T, SIZE>;

}