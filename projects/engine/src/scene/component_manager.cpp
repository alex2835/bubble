
#include "engine/scene/component_manager.hpp"

namespace bubble
{
ComponentManager& ComponentManager::Instance()
{
    static ComponentManager componentManager;
    return componentManager;
}

void ComponentManager::AddOnDraw( string_view componentName, OnComponentDrawFunc drawFunc )
{
    auto& storage = Instance();
    auto iter = storage.mComponentFunctions.find( componentName );
    if ( iter == storage.mComponentFunctions.end() )
        iter = storage.mComponentFunctions.emplace( componentName, ComponentFunctions{} ).first;
    iter->second.mOnDraw = drawFunc;
}

OnComponentDrawFunc ComponentManager::GetOnDraw( string_view componentName )
{
    auto& storage = Instance();
    auto iter = storage.mComponentFunctions.find( componentName );
    if ( iter != storage.mComponentFunctions.end() )
        return iter->second.mOnDraw;
    return nullptr;
}

void ComponentManager::AddFromJson( string_view componentName, ComponentFromJson fromJson )
{
    auto& storage = Instance();
    auto iter = storage.mComponentFunctions.find( componentName );
    if ( iter == storage.mComponentFunctions.end() )
        iter = storage.mComponentFunctions.emplace( componentName, ComponentFunctions{} ).first;
    iter->second.mFromJson = fromJson;
}

ComponentFromJson ComponentManager::GetFromJson( string_view componentName )
{
    auto& storage = Instance();
    auto iter = storage.mComponentFunctions.find( componentName );
    if ( iter != storage.mComponentFunctions.end() )
        return iter->second.mFromJson;
    return nullptr;
}

void ComponentManager::AddToJson( string_view componentName, ComponentToJson toJson )
{
    auto& storage = Instance();
    auto iter = storage.mComponentFunctions.find( componentName );
    if ( iter == storage.mComponentFunctions.end() )
        iter = storage.mComponentFunctions.emplace( componentName, ComponentFunctions{} ).first;
    iter->second.mToJson = toJson;
}

ComponentToJson ComponentManager::GetToJson( string_view componentName )
{
    auto& storage = Instance();
    auto iter = storage.mComponentFunctions.find( componentName );
    if ( iter != storage.mComponentFunctions.end() )
        return iter->second.mToJson;
    return nullptr;
}

}
