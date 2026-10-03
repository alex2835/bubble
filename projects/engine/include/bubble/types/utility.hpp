#pragma once
#include <expected>
#include <optional>
#include <utility>

namespace bubble
{
template <typename F, typename S>
using pair = std::pair<F, S>;

template <typename T>
using opt = std::optional<T>;

template <typename T, typename E>
using expected = std::expected<T, E>;
}
