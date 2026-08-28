#pragma once
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