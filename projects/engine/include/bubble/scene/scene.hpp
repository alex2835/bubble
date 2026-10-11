#pragma once
#include "bubble/scene/entity.hpp"
#include "bubble/scene/uid.hpp"
#include "bubble/types/containers.hpp"
#include "bubble/types/opt_ref.hpp"
#include "bubble/types/utility.hpp"
#include <entt/entity/registry.hpp>

namespace bubble
{
// The entities of a level or a prefab and their components, in an EnTT
// registry. Every entity has a Uid - a new one when it is created, the one it
// had when it is loaded - and the scene finds an entity by it.
//
// A component stays where it is while other entities get one of its type:
// EnTT keeps components in pages and never moves one to make room. Removing
// one moves the last of its type into the hole, though, so a reference from
// Get is good until the next removal of that type; what lasts is the Entity.
class Scene
{
public:
    // Uids from a random seed.
    Scene();
    // Uids from `seed`: the same seed gives the same uids, for tests and
    // replays.
    explicit Scene( u64 seed );

    // A new entity with a new uid.
    Entity Create();
    // An entity under the uid it had - from a file, or a removal undone.
    // Fails for an empty uid and for one another entity has.
    expected<Entity, string> Create( Uid uid );
    // An entity already gone is left alone.
    void Destroy( Entity entity );
    // False for Entity::Null and for an entity that is gone.
    bool Alive( Entity entity ) const;
    size_t Count() const;

    opt<Entity> Find( Uid uid ) const;
    // Empty for an entity that is gone.
    opt<Uid> UidOf( Entity entity ) const;

    // Components by type. Add replaces one the entity has; adding to an
    // entity that is gone is a bug, a logic_error. The rest find nothing on
    // an entity that is gone.
    template <typename Component, typename... Args>
    Component& Add( Entity entity, Args&&... args )
    {
        RequireAlive( entity, "Add" );
        return mRegistry.emplace_or_replace<Component>( entity, std::forward<Args>( args )... );
    }

    template <typename Component>
    OptRef<Component> Get( Entity entity )
    {
        Component* component = Alive( entity ) ? mRegistry.try_get<Component>( entity ) : nullptr;
        return component ? OptRef<Component>( *component ) : nullopt;
    }

    template <typename Component>
    OptRef<const Component> Get( Entity entity ) const
    {
        const Component* component = Alive( entity ) ? mRegistry.try_get<Component>( entity ) : nullptr;
        return component ? OptRef<const Component>( *component ) : nullopt;
    }

    template <typename Component>
    bool Has( Entity entity ) const
    {
        return Get<Component>( entity ).HasValue();
    }

    template <typename Component>
    void Remove( Entity entity )
    {
        if ( Alive( entity ) )
            mRegistry.remove<Component>( entity );
    }

    // fn( Entity, Components&... ) for every entity that has all of them.
    template <typename... Components, typename F>
    void ForEach( F&& fn )
    {
        mRegistry.view<Components...>().each( fn );
    }

    template <typename... Components, typename F>
    void ForEach( F&& fn ) const
    {
        // A const registry cannot make the storage of a type no entity has
        // had yet; the view over it is then empty-handed, and there is
        // nothing to walk.
        const auto view = mRegistry.view<const Components...>();
        if ( view )
            view.each( fn );
    }

private:
    // The entity's uid, as a component nobody outside can name, add or
    // remove.
    struct UidComponent
    {
        Uid mUid;
    };

    Entity Make( Uid uid );
    Uid NextUid();
    void RequireAlive( Entity entity, string_view what ) const;

    entt::basic_registry<Entity> mRegistry;
    hmap<Uid, Entity> mByUid;
    u64 mUidState = 0;
};
}
