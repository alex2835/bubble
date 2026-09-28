#include "engine/pch/pch.hpp"
#include "engine/editing/commands/component_commands.hpp"
#include "engine/project/project.hpp"
#include "engine/scene/component_manager.hpp"
#include <sol/sol.hpp>
#include "engine/scene/components/state_component.hpp"

namespace bubble
{
/// AddComponentCommand

AddComponentCommand::AddComponentCommand( Entity entity, ComponentTypeId componentId, Project& project, Scene& scene )
    : mEntity( entity ),
      mComponentId( componentId ),
      mProject( project ),
      mScene( scene ),
      mName( std::format( "Add {}", FieldLabel( ComponentManager::GetName( componentId ) ) ) )
{
}

void AddComponentCommand::Execute()
{
    Scene& scene = mScene;
    if ( mComponentId == StateComponent::ID() )
        scene.AddComponent<StateComponent>( mEntity, mProject.mScriptingEngine.CreateTable() );
    else
        scene.AddComponent( mEntity, mComponentId );
}

void AddComponentCommand::Undo()
{
    mScene.RemoveComponent( mEntity, mComponentId );
}

/// RemoveComponentCommand

RemoveComponentCommand::RemoveComponentCommand( Entity entity, ComponentTypeId componentId, Scene& scene )
    : mEntity( entity ),
      mComponentId( componentId ),
      mScene( scene ),
      mName( std::format( "Remove {}", FieldLabel( ComponentManager::GetName( componentId ) ) ) )
{
}

void RemoveComponentCommand::Execute()
{
    // Only the one component is parked, on a stand-in entity of the backup.
    if ( mBackupEntity == Entity::Null )
        mBackupEntity = mBackupScene.CreateEntity();
    mScene.CopyComponent( mEntity, mComponentId, mBackupScene, mBackupEntity );
    mScene.RemoveComponent( mEntity, mComponentId );
}

void RemoveComponentCommand::Undo()
{
    mBackupScene.CopyComponent( mBackupEntity, mComponentId, mScene, mEntity );
    mBackupScene.RemoveComponent( mBackupEntity, mComponentId );
}

}
