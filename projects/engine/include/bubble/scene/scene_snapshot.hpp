#pragma once
#include "bubble/scene/scene.hpp"
#include <entt/core/any.hpp>

namespace bubble
{
class ComponentTypes;

// Every entity's component of type T, copied, in the order the scene walks
// them: what a snapshot keeps of one type.
template <typename T>
entt::any CopyComponents( const Scene& scene )
{
    vector<pair<Entity, T>> copies;
    for ( const Entity entity : scene.EntitiesWith<T>() )
        copies.emplace_back( entity, scene.Get<T>( entity ) );
    return entt::any( std::move( copies ) );
}

// Gives each entity its copy back, in that order.
template <typename T>
void AddComponents( Scene& scene, const entt::any& copies )
{
    for ( const auto& [entity, component] : entt::any_cast<const vector<pair<Entity, T>>&>( copies ) )
        scene.Add<T>( entity, component );
}

// A scene's data copied whole, in memory: a world's snapshot, from which it
// is put back as it was. Not a file - it lasts as long as App's types do and
// means nothing to a level edited meanwhile.
//
// What is copied is what the scene's future depends on: the entities'
// numbers - the free ones too, with the versions they come back under, so
// the next entity made gets the number it would have got - the tree, and
// the components of every type ComponentTypes knows, its internal ones
// included. A world's runtime components are not: its modules build them
// again from the data. Only the order every type's components lay in is
// kept, for all of them, because the order a module walks them in decides
// what comes of it.
class SceneSnapshot
{
public:
    SceneSnapshot( const Scene& scene, const ComponentTypes& componentTypes );

    // Every entity and component `scene` has goes - its listeners hear each
    // component go - and the copies come back under their own numbers,
    // heard coming type by type: the tree first, then ComponentTypes'
    // order. A type ComponentTypes no longer knows is a logic_error.
    void Restore( Scene& scene, const ComponentTypes& componentTypes ) const;
    // Lays every type's components in the order they had when copied - a
    // world calls it once its modules built their runtime again. Entities
    // that were not there then go after the rest.
    void Reorder( Scene& scene ) const;

private:
    struct Copies
    {
        Name mComponent;
        entt::any mComponents;
    };

    struct Order
    {
        entt::id_type mStorage = 0;
        vector<Entity> mEntities;
    };

    // As EnTT keeps them: the live ones, then the free ones in the order
    // they are taken again.
    vector<Entity> mEntities;
    size_t mLive = 0;
    Entity mRoot;
    entt::any mTree;
    vector<Copies> mComponents;
    vector<Order> mOrders;
};
}
