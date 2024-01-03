#pragma once
#include <optional>
#include <type_traits>

namespace recs
{
typedef size_t ComponentTypeID;
constexpr ComponentTypeID INVALID_COMPONENT_TYPE = 0;

template <typename T>
using OptRef = std::optional<std::reference_wrapper<T>>;

template <typename T>
using Opt = std::optional<T>;

template <typename T>
using Ref = std::reference_wrapper<T>;

// ========== Meta programming ========== 

// Tuples
template <typename T, size_t Size, size_t... Is>
decltype( auto ) as_tuple_impl( const T( &array )[Size], std::index_sequence<Is...> )
{
    return std::make_tuple( array[Is]... );
}

template <typename T, size_t Size, typename Indices = std::make_index_sequence<Size>>
decltype( auto ) as_tuple( const T( &array )[Size] )
{
    return as_tuple_impl( array, Indices{} );
}

}