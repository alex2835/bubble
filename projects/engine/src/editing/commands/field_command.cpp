#include "engine/pch/pch.hpp"
#include "engine/editing/commands/field_command.hpp"
#include "engine/scene/component_manager.hpp"
#include "engine/reflection/reflection.hpp"

namespace bubble
{
SetFieldCommand::SetFieldCommand( Scene& scene, Entity entity, ComponentTypeId componentId, string path,
                                  entt::meta_any from, entt::meta_any to )
    : mScene( scene ),
      mEntity( entity ),
      mComponentId( componentId ),
      mPath( std::move( path ) ),
      mName( std::format( "{}.{}", ComponentManager::GetName( componentId ), mPath ) ),
      mFrom( std::move( from ) ),
      mTo( std::move( to ) )
{
}

void SetFieldCommand::Set( const entt::meta_any& value )
{
    entt::meta_any component = ComponentManager::Reflected( mScene, mEntity, mComponentId );
    if ( not component )
        return;
    // A copy: SetField takes the value, and the command keeps its own.
    SetField( component, mPath, entt::meta_any( value ) );
}

}
