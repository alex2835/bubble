
#include "utils/selection.hpp"

namespace bubble
{
// Selection implementation
void Selection::Clear()
{
    mProjectTreeNode.reset();
    mEntities.clear();
    mGroupTransform = Transform{};
}

void Selection::SelectTreeNode( const Ref<ProjectTreeNode>& node, Scene& scene )
{
    Clear();
    mProjectTreeNode = node;

    if ( node )
    {
        FillEntitiesInSubTree( mEntities, node );
        UpdateGroupTransform( scene );
    }
}

void Selection::AddEntity( Entity entity, Scene& scene )
{
    if ( mEntities.insert( entity ).second )
    {
        UpdateGroupTransform( scene );
    }
}

void Selection::AddEntities( const set<Entity>& entities, Scene& scene )
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

void Selection::RemoveEntity( Entity entity, Scene& scene )
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

void Selection::UpdateGroupTransform( Scene& scene )
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

void Selection::ApplyTransformDelta( const vec3& positionDelta, const vec3& rotationDelta, const vec3& scaleDelta, Scene& scene )
{
    for ( auto entity : mEntities )
    {
        if ( scene.HasComponent<TransformComponent>( entity ) )
        {
            auto& trans = scene.GetComponent<TransformComponent>( entity );
            trans.mPosition += positionDelta;
            trans.mRotation += rotationDelta;
            trans.mScale += scaleDelta;
        }
    }

    // Update group transform to match new position
    mGroupTransform.mPosition += positionDelta;
    mGroupTransform.mRotation += rotationDelta;
    mGroupTransform.mScale += scaleDelta;
}

} // namespace bubble