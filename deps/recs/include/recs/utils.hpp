#pragma once
#include <string_view>
#include <type_traits>

namespace recs
{
using ComponentTypeId = size_t;

template<typename T>
concept ComponentType = requires( T component )
{
    { T::ID() } -> std::same_as<size_t>;
};

// Tuples
template <ComponentType T, size_t Size, size_t... Is>
decltype( auto ) as_tuple_impl( const T( &array )[Size], std::index_sequence<Is...> )
{
    return std::make_tuple( array[Is]... );
}

template <ComponentType T, size_t Size, ComponentType Indices = std::make_index_sequence<Size>>
decltype( auto ) as_tuple( const T( &array )[Size] )
{
    return as_tuple_impl( array, Indices{} );
}

}