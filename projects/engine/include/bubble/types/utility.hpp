#pragma once
#include <concepts>
#include <exception>
#include <expected>
#include <initializer_list>
#include <optional>
#include <stdexcept>
#include <tuple>
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

using std::copy_constructible;
using std::current_exception;
using std::default_initializable;
using std::exception_ptr;
using std::exchange;
using std::in_place_type;
using std::initializer_list;
using std::logic_error;
using std::nullopt;
using std::nullopt_t;
using std::rethrow_exception;
using std::tuple;
using std::type_index;
using std::unexpected;
}
