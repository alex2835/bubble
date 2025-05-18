
#include "engine/scene/component_manager.hpp"

namespace bubble
{
ComponentManager& ComponentManager::Instance()
{
    static ComponentManager componentManager;
    return componentManager;
}

bool ComponentManager::CreateComponentTable( int componentID )
{
    auto& storage = Instance();
    return storage.mComponentFuncTable.emplace( componentID, ComponentFunctionsTable{} ).second;
}

ComponentFunctionsTable& ComponentManager::GetComponentTable( int componentID )
{
    auto& storage = Instance();
    auto iter = storage.mComponentFuncTable.find( componentID );
    if ( iter != storage.mComponentFuncTable.end() )
        return iter->second;
    throw std::runtime_error( std::format( "{} doesn't exist in component manager.", (int)componentID ) );
}

void ComponentManager::AddName( int componentID, string_view name )
{
    auto& table = GetComponentTable( componentID );
    table.mName = name;
}

string_view ComponentManager::GetName( int componentID )
{
    return GetComponentTable( componentID ).mName;
}

int ComponentManager::GetID( string_view name )
{
    auto& storage = Instance();
    for ( const auto& [id, table] : storage.mComponentFuncTable )
    {
        if ( table.mName == name )
            return id;
    }
    throw std::runtime_error( std::format( "Invalid component name: {}", name ) );
}

void ComponentManager::AddOnDraw( int componentID, OnComponentDrawFunc drawFunc )
{
    auto& table = GetComponentTable( componentID );
    table.mOnDraw = drawFunc;
}

OnComponentDrawFunc ComponentManager::GetOnDraw( int componentID )
{
    return GetComponentTable( componentID ).mOnDraw;
}

void ComponentManager::AddFromJson( int componentID, ComponentFromJson fromJson )
{
    auto& table = GetComponentTable( componentID );
    table.mFromJson = fromJson;
}

ComponentFromJson ComponentManager::GetFromJson( int componentID )
{
    return GetComponentTable( componentID ).mFromJson;
}

void ComponentManager::AddToJson( int componentID, ComponentToJson toJson )
{
    auto& table = GetComponentTable( componentID );
    table.mToJson = toJson;
}

ComponentToJson ComponentManager::GetToJson( int componentID )
{
    return GetComponentTable( componentID ).mToJson;
}

void ComponentManager::AddCreateLuaBinding( int componentID, ComponentCreateLuaBinding CreateLuaBindingFunc )
{
    auto& table = GetComponentTable( componentID );
    table.mCreateLuaBinding = CreateLuaBindingFunc;
}

ComponentCreateLuaBinding ComponentManager::GetCreateLuaBinding( int componentID )
{
    return GetComponentTable( componentID ).mCreateLuaBinding;
}

}
