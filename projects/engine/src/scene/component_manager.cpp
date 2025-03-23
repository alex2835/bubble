
#include "engine/scene/component_manager.hpp"

namespace bubble
{
ComponentManager& ComponentManager::Instance()
{
    static ComponentManager componentManager;
    return componentManager;
}

bool ComponentManager::CreateComponentTable( size_t componentID )
{
    auto& storage = Instance();
    return storage.mComponentFuncTable.emplace( componentID, ComponentFunctionsTable{} ).second;
}

ComponentFunctionsTable& ComponentManager::GetComponentTable( size_t componentID )
{
    auto& storage = Instance();
    auto iter = storage.mComponentFuncTable.find( componentID );
    if ( iter != storage.mComponentFuncTable.end() )
        return iter->second;
    throw std::runtime_error( std::format( "{} doesn't exist in component manager.", (size_t)componentID ) );
}

void ComponentManager::AddName( size_t componentID, string_view name )
{
    auto& table = GetComponentTable( componentID );
    table.mName = name;
}

string_view ComponentManager::GetName( size_t componentID )
{
    return GetComponentTable( componentID ).mName;
}

size_t ComponentManager::GetID( string_view name )
{
    auto& storage = Instance();
    for ( const auto& [id, table] : storage.mComponentFuncTable )
    {
        if ( table.mName == name )
            return id;
    }
    throw std::runtime_error( std::format( "Invalid component name: {}", name ) );
}

void ComponentManager::AddOnDraw( size_t componentID, OnComponentDrawFunc drawFunc )
{
    auto& table = GetComponentTable( componentID );
    table.mOnDraw = drawFunc;
}

OnComponentDrawFunc ComponentManager::GetOnDraw( size_t componentID )
{
    return GetComponentTable( componentID ).mOnDraw;
}

void ComponentManager::AddFromJson( size_t componentID, ComponentFromJson fromJson )
{
    auto& table = GetComponentTable( componentID );
    table.mFromJson = fromJson;
}

ComponentFromJson ComponentManager::GetFromJson( size_t componentID )
{
    return GetComponentTable( componentID ).mFromJson;
}

void ComponentManager::AddToJson( size_t componentID, ComponentToJson toJson )
{
    auto& table = GetComponentTable( componentID );
    table.mToJson = toJson;
}

ComponentToJson ComponentManager::GetToJson( size_t componentID )
{
    return GetComponentTable( componentID ).mToJson;
}

void ComponentManager::AddCreateLuaBinding( size_t componentID, ComponentCreateLuaBinding CreateLuaBindingFunc )
{
    auto& table = GetComponentTable( componentID );
    table.mCreateLuaBinding = CreateLuaBindingFunc;
}

ComponentCreateLuaBinding ComponentManager::GetCreateLuaBinding( size_t componentID )
{
    return GetComponentTable( componentID ).mCreateLuaBinding;
}

}
