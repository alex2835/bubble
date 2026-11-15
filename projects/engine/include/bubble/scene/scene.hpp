#pragma once
#include "bubble/core/name.hpp"
#include "bubble/scene/entity.hpp"
#include "bubble/types/containers.hpp"
#include "bubble/types/opt_ref.hpp"
#include "bubble/types/string.hpp"
#include "bubble/types/utility.hpp"
#include <entt/entity/registry.hpp>
#include <entt/signal/sigh.hpp>

namespace bubble
{
// Hears that a component of one type came to an entity, was replaced whole
// or went - a World does, for its modules. Called in the middle of the
// change: a listener notes what happened and acts later, and never changes
// the scene from here. A component going is heard while it is still there.
class ComponentListener
{
public:
    virtual void Added( Entity entity ) = 0;
    virtual void Replaced( Entity entity ) = 0;
    virtual void Removed( Entity entity ) = 0;

protected:
    ~ComponentListener() = default;
};

// Hears that a field of an entity's component was set - by
// ComponentTypes::TrySetField, or by code that set it and said so. The
// field is the path's first step: light.shadow.bias is heard as shadow.
class FieldListener
{
public:
    virtual void FieldChanged( Entity entity, Name component, Name field ) = 0;

protected:
    ~FieldListener() = default;
};

// The entities of a level or a prefab and their components, in an EnTT
// registry. An Entity is this run's number; what lasts - in files, links
// between objects, saves - is the entity's path in the tree, which hangs
// from the root every scene is made with (bubble/scene/hierarchy.hpp).
//
// A component stays where it is while other entities get one of its type:
// EnTT keeps components in pages and never moves one to make room. Removing
// one moves the last of its type into the hole, though, so a reference from
// Get is good until the next removal of that type; what lasts is the Entity.
class Scene
{
public:
    Scene();

    Entity Root() const { return mRoot; }
    // An entity outside the tree; CreateChild makes one in it.
    Entity Create();
    // Brings back an entity that is gone under its own number - version and
    // all - with no components: undo puts back what was there, and whatever
    // named it before names it again. A number in use is a logic_error.
    void Revive( Entity entity );
    // With everything under it, out of its parent's children. An entity
    // already gone is left alone; the root goes with the scene only - asking
    // to destroy it is a logic_error.
    void Destroy( Entity entity );
    // False for Entity::Null and for an entity that is gone.
    bool Alive( Entity entity ) const;
    // The root among them.
    size_t Count() const;

    // Components by type. Add replaces one the entity has; adding to an
    // entity that is gone is a bug, a logic_error. Get is for a component
    // that has to be there - one missing is a logic_error too - and TryGet
    // for one that may not be; on an entity that is gone it finds nothing.
    template <typename Component, typename... Args>
    Component& Add( Entity entity, Args&&... args )
    {
        RequireAlive( entity, "Add" );
        return mRegistry.emplace_or_replace<Component>( entity, std::forward<Args>( args )... );
    }

    template <typename Component>
    OptRef<Component> TryGet( Entity entity )
    {
        Component* component = Alive( entity ) ? mRegistry.try_get<Component>( entity ) : nullptr;
        return component ? OptRef<Component>( *component ) : nullopt;
    }

    template <typename Component>
    OptRef<const Component> TryGet( Entity entity ) const
    {
        const Component* component = Alive( entity ) ? mRegistry.try_get<Component>( entity ) : nullptr;
        return component ? OptRef<const Component>( *component ) : nullopt;
    }

    template <typename Component>
    Component& Get( Entity entity )
    {
        const auto componentMaybe = TryGet<Component>( entity );
        if ( not componentMaybe )
            Missing( entity, entt::type_name<Component>::value() );
        return *componentMaybe;
    }

    template <typename Component>
    const Component& Get( Entity entity ) const
    {
        const auto componentMaybe = TryGet<Component>( entity );
        if ( not componentMaybe )
            Missing( entity, entt::type_name<Component>::value() );
        return *componentMaybe;
    }

    template <typename Component>
    bool Has( Entity entity ) const
    {
        return TryGet<Component>( entity ).HasValue();
    }

    template <typename Component>
    void Remove( Entity entity )
    {
        if ( Alive( entity ) )
            mRegistry.remove<Component>( entity );
    }

    // Every entity with a Component, in the order its components lie: a
    // copy, so the loop may change the scene.
    template <typename Component>
    vector<Entity> EntitiesWith() const
    {
        vector<Entity> entities;
        if ( const auto* storage = mRegistry.storage<Component>() )
        {
            // The entities, not the components a storage's own walk gives.
            const entt::basic_sparse_set<Entity>& owners = *storage;
            entities.assign( owners.begin(), owners.end() );
        }
        return entities;
    }

    // `listener` hears components of this type come, get replaced and go,
    // for as long as the scene lives.
    template <typename Component>
    void Listen( ComponentListener& listener )
    {
        auto& storage = mRegistry.storage<Component>();
        storage.on_construct().template connect<&Scene::Constructed>( listener );
        storage.on_update().template connect<&Scene::Updated>( listener );
        storage.on_destroy().template connect<&Scene::Destroyed>( listener );
    }

    // `listener` hears fields set, for as long as the scene lives.
    void ListenToFields( FieldListener& listener )
    {
        entt::sink{ mFieldChanged }.connect<&FieldListener::FieldChanged>( listener );
    }
    // For code that set a field of a component itself: its listeners hear it.
    void NotifyFieldChanged( Entity entity, Name component, Name field );

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
    using Registry = entt::basic_registry<Entity>;

    static void Constructed( ComponentListener& listener, Registry&, Entity entity ) { listener.Added( entity ); }
    static void Updated( ComponentListener& listener, Registry&, Entity entity ) { listener.Replaced( entity ); }
    static void Destroyed( ComponentListener& listener, Registry&, Entity entity ) { listener.Removed( entity ); }

    void RequireAlive( Entity entity, string_view what ) const;
    [[noreturn]] void Missing( Entity entity, string_view component ) const;
    void DestroyWithSubtree( Entity entity );

    Registry mRegistry;
    entt::sigh<void( Entity, Name, Name )> mFieldChanged;
    Entity mRoot;
};
}
