#include "bubble/scene/scene.hpp"
#include "bubble/scene/hierarchy.hpp"
#include "bubble/types/format.hpp"

namespace bubble
{
Scene::Scene() : mRoot( Create() )
{
    Add<Hierarchy>( mRoot );
}

Entity Scene::Create()
{
    return mRegistry.create();
}

void Scene::Destroy( Entity entity )
{
    if ( not Alive( entity ) )
        return;
    if ( entity == mRoot )
        throw logic_error( "Destroy on the root: it goes with the scene" );
    if ( const auto nodeMaybe = TryGet<Hierarchy>( entity ) )
    {
        Hierarchy& parent = Get<Hierarchy>( nodeMaybe->mParent );
        erase( parent.mChildren, entity );
        parent.mChildByName.erase( parent.mChildByName.find( nodeMaybe->mName.View() ) );
    }
    DestroyWithSubtree( entity );
}

void Scene::DestroyWithSubtree( Entity entity )
{
    if ( const auto nodeMaybe = TryGet<Hierarchy>( entity ) )
    {
        // A copy: destroying a child moves another Hierarchy into its slot,
        // and this one may be it.
        const vector<Entity> children = nodeMaybe->mChildren;
        for ( const Entity child : children )
            DestroyWithSubtree( child );
    }
    mRegistry.destroy( entity );
}

bool Scene::Alive( Entity entity ) const
{
    return mRegistry.valid( entity );
}

size_t Scene::Count() const
{
    // The entities' own storage keeps the live ones first, the freed ones
    // after: free_list() is where the freed begin.
    return mRegistry.storage<Entity>()->free_list();
}

void Scene::Missing( Entity entity, string_view component ) const
{
    throw logic_error( format( "Get<{}> on {}, which has none", component, Describe( *this, entity ) ) );
}

void Scene::RequireAlive( Entity entity, string_view what ) const
{
    if ( not Alive( entity ) )
        throw logic_error( format( "{} on entity {}, which is gone", what, static_cast<u32>( entity ) ) );
}
}
