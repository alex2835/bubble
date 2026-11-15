#include "bubble/scene/scene_snapshot.hpp"
#include "bubble/core/profile.hpp"
#include "bubble/scene/component_types.hpp"
#include "bubble/scene/hierarchy.hpp"
#include "bubble/types/format.hpp"

namespace bubble
{
SceneSnapshot::SceneSnapshot( const Scene& scene, const ComponentTypes& componentTypes )
{
    BUBBLE_PROFILE_ZONE();
    const auto& entities = *scene.mRegistry.storage<Entity>();
    mEntities.assign( entities.data(), entities.data() + entities.size() );
    mLive = entities.free_list();
    mRoot = scene.mRoot;

    mTree = CopyComponents<Hierarchy>( scene );
    for ( const span<const ComponentTypes::Type> types : { componentTypes.All(), componentTypes.Internal() } )
        for ( const ComponentTypes::Type& type : types )
            mComponents.push_back( Copies{ type.mName, type.mCopyAll( scene ) } );

    for ( const auto [id, storage] : scene.mRegistry.storage() )
        if ( not storage.empty() )
            mOrders.push_back( Order{ id, vector<Entity>( storage.begin(), storage.end() ) } );
}

void SceneSnapshot::Restore( Scene& scene, const ComponentTypes& componentTypes ) const
{
    BUBBLE_PROFILE_ZONE();
    // Every type looked up first: a snapshot that cannot come back leaves
    // the scene as it is.
    for ( const Copies& copies : mComponents )
        if ( not componentTypes.TryFindAny( copies.mComponent ) )
            throw logic_error(
                format( "SceneSnapshot::Restore: {} is no component type here", copies.mComponent.ToString() ) );

    // The registry's own clear tells the listeners; then the entities'
    // storage is emptied too, the numbers it handed out forgotten, and
    // filled as it was. Taken in order by an empty storage, each entity
    // keeps its number and version, and the next new number is the first
    // none of them has - the one the scene would have used.
    scene.mRegistry.clear();
    auto& entities = scene.mRegistry.storage<Entity>();
    entities.clear();
    entities.reserve( mEntities.size() );
    for ( const Entity entity : mEntities )
        entities.generate( entity );
    entities.free_list( mLive );
    scene.mRoot = mRoot;

    AddComponents<Hierarchy>( scene, mTree );
    for ( const Copies& copies : mComponents )
        componentTypes.TryFindAny( copies.mComponent )->mAddAll( scene, copies.mComponents );
    Reorder( scene );
}

void SceneSnapshot::Reorder( Scene& scene ) const
{
    BUBBLE_PROFILE_ZONE();
    for ( const Order& order : mOrders )
        if ( auto* storage = scene.mRegistry.storage( order.mStorage ) )
            storage->sort_as( order.mEntities.begin(), order.mEntities.end() );
}
}
