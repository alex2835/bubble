#pragma once
#include <string_view>
#include <type_traits>

namespace recs
{
typedef size_t ComponentTypeId;
constexpr ComponentTypeId INVALID_COMPONENT_TYPE = 0;

template<typename T>
concept ComponentType = requires( T component )
{
    { T::Name() } -> std::same_as<std::string_view>;
};

struct string_hash
{
    using is_transparent = void;
    [[nodiscard]] size_t operator()( const char* txt ) const
    {
        return std::hash<std::string_view>{}( txt );
    }
    [[nodiscard]] size_t operator()( std::string_view txt ) const
    {
        return std::hash<std::string_view>{}( txt );
    }
    [[nodiscard]] size_t operator()( const std::string& txt ) const
    {
        return std::hash<std::string>{}( txt );
    }
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