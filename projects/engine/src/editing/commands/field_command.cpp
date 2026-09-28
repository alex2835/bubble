#include "engine/pch/pch.hpp"
#include "engine/editing/commands/field_command.hpp"
#include "engine/scene/component_manager.hpp"
#include "engine/reflection/reflection.hpp"
#include "engine/scene/hierarchy.hpp"

namespace bubble
{
ComponentField ParseComponentField( string_view path )
{
    const size_t dot = path.find( '.' );
    if ( dot == string_view::npos or dot == 0 or dot + 1 == path.size() )
        throw std::runtime_error( std::format( "'{}' is not Component.field", path ) );
    // GetID throws on a name no component has, naming it.
    return { ComponentManager::GetID( path.substr( 0, dot ) ), string( path.substr( dot + 1 ) ) };
}

entt::meta_any RequireReflected( Scene& scene, Entity entity, ComponentTypeId componentId )
{
    const string_view name = ComponentManager::GetName( componentId );
    if ( not scene.HasEntity( entity ) )
        throw std::runtime_error( std::format( "no entity {}", (u64)entity ) );
    if ( not scene.HasComponent( entity, componentId ) )
        throw std::runtime_error( std::format( "{} has no {} component", DescribeEntity( scene, entity ), name ) );
    entt::meta_any component = ComponentManager::Reflected( scene, entity, componentId );
    if ( not component )
        throw std::runtime_error( std::format( "{} does not describe its fields yet", name ) );
    return component;
}

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
