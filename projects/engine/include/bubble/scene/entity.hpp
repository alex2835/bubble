#pragma once
#include "bubble/types/function.hpp"
#include "bubble/types/number.hpp"

namespace bubble
{
// An entity of a Scene, numbered as EnTT numbers its own: 20 bits of index
// and 12 of version. An index is reused once its entity is gone, with the
// version moved on, so an Entity kept past its removal never names the one
// made in its place - Scene::Alive says no. A handle by nature, for this run
// only: what a file keeps is the entity's path in the tree.
//
// A class rather than EnTT's enum, so it can have a Luau type of its own.
// EnTT takes any class with an entity_type and the two conversions.
class Entity
{
public:
    using entity_type = u32;

    // No entity, what EnTT calls entt::null; what a default Entity is.
    static const Entity Null;

    constexpr Entity() = default;
    constexpr explicit Entity( entity_type value ) : mValue( value ) {}
    constexpr explicit operator entity_type() const { return mValue; }

    friend constexpr bool operator==( Entity, Entity ) = default;

private:
    entity_type mValue = ~entity_type( 0 );
};

inline constexpr Entity Entity::Null{};
}

template <>
struct std::hash<bubble::Entity>
{
    size_t operator()( bubble::Entity entity ) const { return static_cast<bubble::u32>( entity ); }
};
