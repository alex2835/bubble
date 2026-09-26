#include "engine/pch/pch.hpp"
#include "engine/editing/selection.hpp"
#include "engine/scene/components/transform_component.hpp"
#include "engine/utils/error.hpp"

namespace bubble
{
// Selection implementation
void Selection::Prune( const Scene& scene, const Ref<ProjectTreeNode>& root )
{
    if ( mProjectTreeNode and mProjectTreeNode != root and not FindNodeById( mProjectTreeNode->ID(), root ) )
        mProjectTreeNode = nullptr;

    bool dropped = false;
    for ( auto it = mEntities.begin(); it != mEntities.end(); )
    {
        if ( scene.HasEntity( *it ) )
            ++it;
        else
        {
            it = mEntities.erase( it );
            dropped = true;
        }
    }
    if ( dropped )
        UpdateGroupTransform( scene );
}

void Selection::Clear()
{
    mProjectTreeNode.reset();
    mEntities.clear();
    mGroupTransform = Transform{};
}

void Selection::SelectTreeNode( const Ref<ProjectTreeNode>& node, const Scene& scene )
{
    Clear();
    mProjectTreeNode = node;

    if ( node )
    {
        FillEntitiesInSubTree( mEntities, node );
        UpdateGroupTransform( scene );
    }
}

void Selection::AddEntity( Entity entity, const Scene& scene )
{
    if ( mEntities.insert( entity ).second )
    {
        UpdateGroupTransform( scene );
    }
}

void Selection::AddEntities( const set<Entity>& entities, const Scene& scene )
{
    bool changed = false;
    for ( auto entity : entities )
    {
        if ( mEntities.insert( entity ).second )
            changed = true;
    }

    if ( changed )
        UpdateGroupTransform( scene );
}

void Selection::RemoveEntity( Entity entity, const Scene& scene )
{
    if ( mEntities.erase( entity ) > 0 )
    {
        UpdateGroupTransform( scene );
    }
}

Entity Selection::GetSingleEntity() const
{
    BUBBLE_ASSERT( IsSingleSelection(), "GetSingleEntity called when not exactly one entity selected" );
    return *mEntities.begin();
}

void Selection::UpdateGroupTransform( const Scene& scene )
{
    if ( mEntities.empty() )
    {
        mGroupTransform = Transform{};
        return;
    }

    vec3 avgPos( 0 );
    int count = 0;

    for ( auto entity : mEntities )
    {
        if ( scene.HasComponent<TransformComponent>( entity ) )
        {
            const auto& trans = scene.GetComponent<TransformComponent>( entity );
            avgPos += trans.mPosition;
            count++;
        }
    }

    if ( count > 0 )
    {
        mGroupTransform.mPosition = avgPos / static_cast<f32>( count );
    }
    else
    {
        mGroupTransform = Transform{};
    }
}

} // namespace bubble