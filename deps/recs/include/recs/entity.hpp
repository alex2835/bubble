#pragma once
#include <cstdint>
#include <string_view>
#include <functional>

namespace recs
{
class Registry;

/**
 * @brief Entity is a reference to
 * components that stored in pools
 */
class Entity
{
public:
    Entity() = default;
    ~Entity() = default;
    Entity( const Entity& ) = default;
    Entity& operator = ( const Entity& ) = default;

    bool operator == ( Entity other ) const
    {
        return mID == other.mID;
    }
    bool operator <  ( Entity other ) const
    {
        return mID < other.mID;
    }

    bool IsValid()
    {
        return mID;
    }

    template <typename T, typename ...Args>
    Entity AddComponet( Args&& ...args );

    template <typename T>
    T& GetComponent();

    template <typename T>
    bool HasComponent();

    template <typename ...Components>
    bool HasComponents();

    template <typename ...Components>
    std::tuple<Components&...> GetComponents();

    template <typename T>
    Entity RemoveComponet();

    operator size_t() const
    {
        return mID;
    };

private:
    Entity( size_t id, Registry* registry )
        : mID( id ),
        mRegistry( registry )
    {
    }

private:
    size_t mID = 0u;
    Registry* mRegistry = nullptr;
    friend class RegistryMeta;
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
