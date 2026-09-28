#pragma once
#include <cstdint>
#include <functional>

namespace bubble
{
// A handle to an entity of a Scene: 20 bits of index and 12 of version, as
// EnTT lays out its own entt::entity. A class rather than that enum so Lua
// can bind it as a usertype with methods; EnTT takes any class with an
// entity_type and the two conversions below.
//
// An index is reused once its entity is gone, with the version moved on, so
// a handle kept past its entity's removal never names the one made in its
// place: Scene::HasEntity says no.
class Entity
{
public:
    using entity_type = std::uint32_t;

    // No entity: all bits set, what EnTT calls entt::null. What a default
    // made Entity is.
    static const Entity Null;

    constexpr Entity() = default;
    constexpr explicit Entity( entity_type id ) : mId( id ) {}

    // The handle for an id from outside - a file, Lua, the operators' JSON.
    // Null when the id cannot be one. Not necessarily alive: ask the scene.
    static constexpr Entity FromId( std::uint64_t id )
    {
        return id > entity_type( ~entity_type( 0 ) ) ? Entity() : Entity( static_cast<entity_type>( id ) );
    }

    // The whole identifier, version included: what a file, the Lua side and
    // the picking buffer hold.
    constexpr operator entity_type() const { return mId; }

    constexpr bool operator==( const Entity& ) const = default;

private:
    entity_type mId = ~entity_type( 0 );
};

inline constexpr Entity Entity::Null{};

}

template <>
struct std::hash<bubble::Entity>
{
    std::size_t operator()( const bubble::Entity& entity ) const noexcept
    {
        return static_cast<bubble::Entity::entity_type>( entity );
    }
};
