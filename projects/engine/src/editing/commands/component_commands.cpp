#include "engine/pch/pch.hpp"
#include "engine/editing/commands/component_commands.hpp"
#include "engine/project/project.hpp"
#include "engine/scene/component_manager.hpp"
#include <sol/sol.hpp>

namespace bubble
{
/// AddComponentCommand

AddComponentCommand::AddComponentCommand( Entity entity, ComponentTypeId componentId, Project& project )
    : mEntity( entity ),
      mComponentId( componentId ),
      mProject( project ),
      mName( std::format( "Add {}", ComponentManager::GetName( componentId ) ) )
{
}

void AddComponentCommand::Execute()
{
    Scene& scene = mProject.mLevel.mScene;
    if ( mComponentId == StateComponent::ID() )
        scene.AddComponent<StateComponent>( mEntity, mProject.mScriptingEngine.CreateTable() );
    else
        scene.EntityAddComponentId( mEntity, mComponentId );
}

void AddComponentCommand::Undo()
{
    mProject.mLevel.mScene.EntityRemoveComponentId( mEntity, mComponentId );
}

/// RemoveComponentCommand

RemoveComponentCommand::RemoveComponentCommand( Entity entity, ComponentTypeId componentId, Scene& scene )
    : mEntity( entity ),
      mComponentId( componentId ),
      mScene( scene ),
      mName( std::format( "Remove {}", ComponentManager::GetName( componentId ) ) )
{
}

void RemoveComponentCommand::Execute()
{
    // Only the one component is parked, on a stand-in entity of the backup.
    if ( mBackupEntity == INVALID_ENTITY )
        mBackupEntity = mBackupScene.CreateEntity();
    mScene.CopyComponentInto( mBackupScene, mEntity, mComponentId, mBackupEntity );
    mScene.EntityRemoveComponentId( mEntity, mComponentId );
}

void RemoveComponentCommand::Undo()
{
    mBackupScene.CopyComponentInto( mScene, mBackupEntity, mComponentId, mEntity );
    mBackupScene.EntityRemoveComponentId( mBackupEntity, mComponentId );
}

}
