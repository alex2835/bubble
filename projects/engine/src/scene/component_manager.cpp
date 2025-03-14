
#include "engine/scene/components_manager.hpp"

namespace bubble
{
ComponentManager& ComponentManager::Instance()
{
    static ComponentManager componentManager;
    return componentManager;
}

bool ComponentManager::CreateComponentTable( string_view componentName )
{
    auto& storage = Instance();
    return storage.mComponentFuncTable.emplace( componentName, ComponentFunctionsTable{} ).second;
}

ComponentFunctionsTable& ComponentManager::GetComponentTable( string_view componentName )
{
    auto& storage = Instance();
    auto iter = storage.mComponentFuncTable.find( componentName );
    if ( iter != storage.mComponentFuncTable.end() )
        return iter->second;
    throw std::runtime_error( std::format( "{} doesn't exist in component manager.", componentName ) );
}


void ComponentManager::AddOnDraw( string_view componentName, OnComponentDrawFunc drawFunc )
{
    auto& table = GetComponentTable( componentName );
    table.mOnDraw = drawFunc;
}

OnComponentDrawFunc ComponentManager::GetOnDraw( string_view componentName )
{
    return GetComponentTable( componentName ).mOnDraw;
}

void ComponentManager::AddFromJson( string_view componentName, ComponentFromJson fromJson )
{
    auto& table = GetComponentTable( componentName );
    table.mFromJson = fromJson;
}

ComponentFromJson ComponentManager::GetFromJson( string_view componentName )
{
    return GetComponentTable( componentName ).mFromJson;
}

void ComponentManager::AddToJson( string_view componentName, ComponentToJson toJson )
{
    auto& table = GetComponentTable( componentName );
    table.mToJson = toJson;
}

ComponentToJson ComponentManager::GetToJson( string_view componentName )
{
    return GetComponentTable( componentName ).mToJson;
}

void ComponentManager::AddCreateLuaBinding( string_view componentName, ComponentCreateLuaBinding CreateLuaBindingFunc )
{
    auto& table = GetComponentTable( componentName );
    table.mCreateLuaBinding = CreateLuaBindingFunc;
}

ComponentCreateLuaBinding ComponentManager::GetCreateLuaBinding( string_view componentName )
{
    return GetComponentTable( componentName ).mCreateLuaBinding;
}

}
