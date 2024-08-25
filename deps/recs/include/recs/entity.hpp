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

    bool operator == ( Entity other ) const
    {
        return mId == other.mId;
    }
    bool operator <  ( Entity other ) const
    {
        return mId < other.mId;
    }

    bool IsValid()
    {
        return mId && mRegistry;
    }

    // Component type API
    template <ComponentType T, typename ...Args>
    Entity AddComponet( Args&& ...args );

    template <ComponentType T>
    T& GetComponent();

    template <ComponentType T>
    bool HasComponent();

    template <ComponentType ...Components>
    bool HasComponents();

    template <ComponentType ...Components>
    std::tuple<Components&...> GetComponents();

    template <ComponentType T>
    Entity RemoveComponent();

    operator size_t() const
    {
        return mId;
    };

    // Component Ids API
    const std::unordered_set<ComponentTypeId>& EntityComponentTypeIds();
    void EntityAddComponentId( ComponentTypeId componentId );
    void EntityRemoveComponentId( ComponentTypeId componentId );

private:
    Entity( size_t id, Registry* registry )
        : mId( id ),
          mRegistry( registry )
    {}

private:
    size_t mId = 0u;
    Registry* mRegistry = nullptr;
    friend class Registry;
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

