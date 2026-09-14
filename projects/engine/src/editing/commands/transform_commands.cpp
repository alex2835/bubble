#include "engine/pch/pch.hpp"
#include "engine/editing/commands/transform_commands.hpp"

namespace bubble
{
/// TransformChangeCommand

namespace
{
void SetTransform( Scene& scene, Entity entity, const Transform& t )
{
    if ( not scene.HasComponent<TransformComponent>( entity ) )
        return;
    auto& transform = scene.GetComponent<TransformComponent>( entity );
    transform.mPosition = t.mPosition;
    transform.mRotation = t.mRotation;
    transform.mScale = t.mScale;
}
}

TransformChangeCommand::TransformChangeCommand( Entity entity, Scene& scene,
                                                const Transform& oldTransform, const Transform& newTransform )
    : mEntity( entity ),
      mScene( scene ),
      mOldTransform( oldTransform ),
      mNewTransform( newTransform )
{
}

void TransformChangeCommand::Execute() { SetTransform( mScene, mEntity, mNewTransform ); }
void TransformChangeCommand::Undo() { SetTransform( mScene, mEntity, mOldTransform ); }

MultiTransformChangeCommand::MultiTransformChangeCommand( const set<Entity>& entities,
                                                          Scene& scene,
                                                          const map<Entity, Transform>& oldTransforms,
                                                          const map<Entity, Transform>& newTransforms )
    : mEntities( entities ),
      mScene( scene ),
      mOldTransforms( oldTransforms ),
      mNewTransforms( newTransforms )
{
}

void MultiTransformChangeCommand::Execute()
{
    for ( const auto entity : mEntities )
        if ( const auto it = mNewTransforms.find( entity ); it != mNewTransforms.end() )
            SetTransform( mScene, entity, it->second );
}

void MultiTransformChangeCommand::Undo()
{
    for ( const auto entity : mEntities )
        if ( const auto it = mOldTransforms.find( entity ); it != mOldTransforms.end() )
            SetTransform( mScene, entity, it->second );
}

}
