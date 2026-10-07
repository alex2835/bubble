#pragma once
#include <expected>
#include <initializer_list>
#include <optional>
#include <stdexcept>
#include <typeindex>
#include <utility>

namespace bubble
{
template <typename F, typename S>
using pair = std::pair<F, S>;

template <typename T>
using opt = std::optional<T>;

template <typename T, typename E>
using expected = std::expected<T, E>;

using std::exchange;
using std::initializer_list;
using std::logic_error;
using std::nullopt;
using std::nullopt_t;
using std::type_index;
using std::unexpected;
}
