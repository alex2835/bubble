#pragma once
#include <cstdint>
#include <string_view>
#include <functional>
#include <unordered_set>

#include "recs/impex.hpp"

namespace recs
{
class Registry;

/**
 * @brief Entity is a reference to
 * components that stored in pools
 */
class RECS_EXPORT Entity
{
public:
    Entity() = default;
    ~Entity() = default;
    Entity( const Entity& ) = default;
    Entity& operator = ( const Entity& ) = default;

    operator size_t() const
    {
        return mId;
    };

private:
    friend Registry;
    Entity( size_t id )
        : mId( id )
    {}

private:
    size_t mId = 0u;
};

constexpr Entity INVALID_ENTITY = {};

}


template<>
struct std::hash<recs::Entity>
{
    std::size_t operator()( const recs::Entity& entity ) const noexcept
    {
        return (size_t)entity;
    }
};

