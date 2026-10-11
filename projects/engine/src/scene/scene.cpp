#include "bubble/scene/scene.hpp"
#include "bubble/types/format.hpp"

namespace bubble
{
Entity Scene::Create()
{
    return mRegistry.create();
}

void Scene::Destroy( Entity entity )
{
    if ( Alive( entity ) )
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

void Scene::RequireAlive( Entity entity, string_view what ) const
{
    if ( not Alive( entity ) )
        throw logic_error( format( "{} on entity {}, which is gone", what, static_cast<u32>( entity ) ) );
}
}
