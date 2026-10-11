#pragma once
#include <variant>

namespace bubble
{
template <typename... Types>
using variant = std::variant<Types...>;

using std::get_if;
using std::holds_alternative;
using std::visit;
}
