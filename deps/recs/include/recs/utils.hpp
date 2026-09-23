#pragma once
#include <concepts>
#include <format>
#include <string>
#include <string_view>
#include <type_traits>

namespace recs
{
using ComponentTypeId = int;
constexpr ComponentTypeId INVALID_COMPONENT_TYPE_ID = -1;

template<typename T>
concept ComponentType = requires( T component )
{
    { T::ID() } -> std::same_as<int>;
};

// Human-readable component name for error messages: "Model (12)" when the
// type has a static Name(), otherwise just the id.
template <ComponentType T>
std::string ComponentName()
{
    if constexpr ( requires { { T::Name() } -> std::convertible_to<std::string_view>; } )
        return std::format( "{} ({})", std::string_view( T::Name() ), T::ID() );
    else
        return std::to_string( T::ID() );
}

// Tuples
template <ComponentType T, size_t Size, size_t... Is>
decltype( auto ) as_tuple_impl( const T( &array )[Size], std::index_sequence<Is...> )
{
    return std::make_tuple( array[Is]... );
}

// NOTE: unused. Indices is an index_sequence, so it must not be constrained by
// ComponentType - that made this template impossible to instantiate.
template <ComponentType T, size_t Size, typename Indices = std::make_index_sequence<Size>>
decltype( auto ) as_tuple( const T( &array )[Size] )
{
    return as_tuple_impl( array, Indices{} );
}

}