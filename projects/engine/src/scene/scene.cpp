#include "engine/pch/pch.hpp"
#include "engine/scene/scene.hpp"
#include "engine/scene/component_manager.hpp"
#include "engine/loader/asset_reflection.hpp"
#include "engine/scripting/reflection_lua.hpp"
#include <sol/sol.hpp>
#include "engine/scene/components/audio_listener_component.hpp"
#include "engine/scene/components/audio_source_component.hpp"
#include "engine/scene/components/camera_component.hpp"
#include "engine/scene/components/character_controller_component.hpp"
#include "engine/scene/components/light_component.hpp"
#include "engine/scene/components/model_component.hpp"
#include "engine/scene/components/rigid_body_component.hpp"
#include "engine/scene/components/animator_component.hpp"
#include "engine/scene/components/script_component.hpp"
#include "engine/scene/components/shader_component.hpp"
#include "engine/scene/components/state_component.hpp"
#include "engine/scene/components/tag_component.hpp"
#include "engine/scene/components/transform_component.hpp"
#include "engine/scene/components/hierarchy_component.hpp"
#include "engine/scene/components/prefab_instance_component.hpp"
#include "engine/scene/components/folder_component.hpp"

namespace bubble
{
namespace
{
// An entity as a field's value: its id in a file, null for none; the entity
// usertype in Lua.
void ReflectEntity()
{
    entt::meta_factory<Entity>{}.type( "entity" );
    RegisterJsonCodec(
        entt::resolve<Entity>(),
        []( const entt::meta_any& value, const ReflectionContext& ) -> json
        {
            const Entity entity = value.cast<Entity>();
            return entity == Entity::Null ? json( nullptr ) : json( (u64)entity );
        },
        []( const json& j, const ReflectionContext& ) -> entt::meta_any
        {
            if ( j.is_null() )
                return Entity::Null;
            if ( not j.is_number_unsigned() and not j.is_number_integer() )
                throw std::runtime_error( std::format( "{} is not an entity id", j.dump() ) );
            return Entity::FromId( j.get<u64>() );
        } );
    RegisterLuaValue<Entity>();
}

// Once per process, before the first scene: the id API needs every type.
void RegisterComponents()
{
    // Before the components: their fields hold resources and entities.
    ReflectAssets();
    ReflectEntity();
    ComponentManager::Add<TagComponent>();
    ComponentManager::Add<ModelComponent>();
    ComponentManager::Add<TransformComponent>();
    ComponentManager::Add<ShaderComponent>();
    ComponentManager::Add<CameraComponent>();
    ComponentManager::Add<LightComponent>();
    ComponentManager::Add<ScriptComponent>();
    ComponentManager::Add<RigidBodyComponent>();
    ComponentManager::Add<CharacterControllerComponent>();
    ComponentManager::Add<StateComponent>();
    ComponentManager::Add<AudioSourceComponent>();
    ComponentManager::Add<AudioListenerComponent>();
    ComponentManager::Add<AnimatorComponent>();
    ComponentManager::Add<HierarchyComponent>();
    ComponentManager::Add<PrefabInstanceComponent>();
    ComponentManager::Add<FolderComponent>();
}
}

Scene::Scene()
{
    static const bool registered = ( RegisterComponents(), true );
    (void)registered;
}

Scene::Storage* Scene::FindStorage( ComponentTypeId componentId )
{
    return mRegistry.storage( ComponentManager::Get( componentId ).mStorageId );
}

const Scene::Storage* Scene::FindStorage( ComponentTypeId componentId ) const
{
    return mRegistry.storage( ComponentManager::Get( componentId ).mStorageId );
}

Scene::Storage& Scene::EnsureStorage( ComponentTypeId componentId )
{
    return ComponentManager::Get( componentId ).mStorage( mRegistry );
}

void Scene::RequireEntity( Entity entity, std::string_view what ) const
{
    if ( not HasEntity( entity ) )
        throw std::runtime_error( std::format( "{}: no entity {}", what, (size_t)entity ) );
}

/// Entities

Entity Scene::CreateEntity()
{
    return mRegistry.create();
}

Entity Scene::CreateEntity( Entity id )
{
    if ( id == Entity::Null or HasEntity( id ) )
        throw std::runtime_error( std::format( "CreateEntity: {} is taken", (size_t)id ) );
    const Entity made = mRegistry.create( id );
    if ( made != id )
    {
        mRegistry.destroy( made );
        throw std::runtime_error( std::format( "CreateEntity: {} is not free", (size_t)id ) );
    }
    return made;
}

void Scene::RemoveEntity( Entity entity )
{
    RequireEntity( entity, "RemoveEntity" );
    mRegistry.destroy( entity );
}

void Scene::RemoveEntities( std::span<const Entity> entities )
{
    for ( const Entity entity : entities )
        if ( HasEntity( entity ) )
            mRegistry.destroy( entity );
}

size_t Scene::EntityCount() const
{
    return mRegistry.storage<Entity>()->free_list();
}

Entity Scene::CopyEntity( Entity entity, Scene& target, Entity id )
{
    RequireEntity( entity, "CopyEntity" );
    const Entity made = id == Entity::Null ? target.CreateEntity() : target.CreateEntity( id );
    for ( const ComponentTypeId componentId : ComponentsOf( entity ) )
        CopyComponent( entity, componentId, target, made );
    return made;
}

/// Components by id

void* Scene::AddComponent( Entity entity, ComponentTypeId componentId )
{
    RequireEntity( entity, "AddComponent" );
    Storage& storage = EnsureStorage( componentId );
    if ( storage.contains( entity ) )
        throw std::runtime_error( std::format( "AddComponent: entity {} has component {} already", (size_t)entity, componentId ) );
    if ( storage.push( entity ) == storage.end() )
        throw std::runtime_error( std::format( "AddComponent: component {} cannot be made", componentId ) );
    return storage.value( entity );
}

void* Scene::TryGetComponent( Entity entity, ComponentTypeId componentId )
{
    Storage* storage = FindStorage( componentId );
    return storage and storage->contains( entity ) ? storage->value( entity ) : nullptr;
}

const void* Scene::TryGetComponent( Entity entity, ComponentTypeId componentId ) const
{
    return const_cast<Scene*>( this )->TryGetComponent( entity, componentId );
}

bool Scene::HasComponent( Entity entity, ComponentTypeId componentId ) const
{
    const Storage* storage = FindStorage( componentId );
    return storage and storage->contains( entity );
}

void Scene::RemoveComponent( Entity entity, ComponentTypeId componentId )
{
    if ( Storage* storage = FindStorage( componentId ) )
        storage->remove( entity );
}

void Scene::CopyComponent( Entity entity, ComponentTypeId componentId, Scene& target, Entity targetEntity )
{
    target.RequireEntity( targetEntity, "CopyComponent" );
    const void* source = TryGetComponent( entity, componentId );
    if ( not source )
        throw std::runtime_error( std::format( "CopyComponent: entity {} has no component {}", (size_t)entity, componentId ) );
    Storage& to = target.EnsureStorage( componentId );
    if ( to.contains( targetEntity ) )
        throw std::runtime_error( std::format( "CopyComponent: entity {} has component {} already", (size_t)targetEntity, componentId ) );
    // Copy constructed from the source. Components live in pages, so making
    // room in the same storage (a copy within the scene) leaves the source
    // where it is.
    if ( to.push( targetEntity, source ) == to.end() )
        throw std::runtime_error( std::format( "CopyComponent: component {} cannot be copied", componentId ) );
}

vector<ComponentTypeId> Scene::ComponentsOf( Entity entity ) const
{
    vector<ComponentTypeId> ids;
    for ( const ComponentTypeId id : ComponentManager::Ids() )
        if ( HasComponent( entity, id ) )
            ids.push_back( id );
    return ids;
}

}
