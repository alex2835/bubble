#pragma once
#include <utility>
#include <optional>
#include <future>

namespace bubble
{
template <typename F, typename S>
using pair = std::pair<F, S>;

template <typename T>
using opt = std::optional<T>;

template <typename T>
using future = std::future<T>;

}

