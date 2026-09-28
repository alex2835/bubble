#include "engine/pch/pch.hpp"
#include "engine/scene/component_manager.hpp"
#include "engine/project/project.hpp"

namespace bubble
{
ComponentManager& ComponentManager::Instance()
{
    static ComponentManager componentManager;
    return componentManager;
}

void ComponentManager::Register( ComponentTypeId componentId, const ComponentFunctionsTable& table )
{
    auto& manager = Instance();
    if ( not manager.mComponentFuncTable.emplace( componentId, table ).second )
        return;
    manager.mIds.insert( std::ranges::upper_bound( manager.mIds, componentId ), componentId );
}

const vector<ComponentTypeId>& ComponentManager::Ids()
{
    return Instance().mIds;
}

const ComponentFunctionsTable& ComponentManager::Get( ComponentTypeId componentId )
{
    auto& manager = Instance();
    const auto iter = manager.mComponentFuncTable.find( componentId );
    if ( iter != manager.mComponentFuncTable.end() )
        return iter->second;
    throw std::runtime_error( std::format( "{} doesn't exist in component manager.", componentId ) );
}

entt::meta_any ComponentManager::Reflected( Scene& scene, Entity entity, ComponentTypeId componentId )
{
    const entt::meta_type& meta = Get( componentId ).mMeta;
    void* component = meta ? scene.TryGetComponent( entity, componentId ) : nullptr;
    return component ? meta.from_void( component ) : entt::meta_any{};
}

ReflectionContext ComponentManager::ContextOf( const Project& project )
{
    // The loader caches what it loads; handing it out from a const project is
    // how every load from a file already reaches it.
    auto& mutableProject = const_cast<Project&>( project );
    return { &mutableProject.mLoader, &mutableProject.mScriptingEngine };
}

string_view ComponentManager::GetName( ComponentTypeId componentId )
{
    return Get( componentId ).mName;
}

ComponentTypeId ComponentManager::GetID( string_view name )
{
    for ( const auto& [id, table] : Instance().mComponentFuncTable )
        if ( table.mName == name )
            return id;
    throw std::runtime_error( std::format( "Invalid component name: {}", name ) );
}

OnComponentDrawFunc ComponentManager::GetOnDraw( ComponentTypeId componentId )
{
    return Get( componentId ).mOnDraw;
}

ComponentFromJson ComponentManager::GetFromJson( ComponentTypeId componentId )
{
    return Get( componentId ).mFromJson;
}

ComponentToJson ComponentManager::GetToJson( ComponentTypeId componentId )
{
    return Get( componentId ).mToJson;
}

ComponentCreateLuaBinding ComponentManager::GetCreateLuaBinding( ComponentTypeId componentId )
{
    return Get( componentId ).mCreateLuaBinding;
}

}
