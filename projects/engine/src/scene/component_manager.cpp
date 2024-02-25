
#include "engine/scene/component_manager.hpp"

namespace bubble
{

void ComponentManager::AddOnDraw( const string& componentName, OnComponentDrawFunc drawFunc )
{
    mComponentFunctions[componentName].mOnDraw = drawFunc;
}

OnComponentDrawFunc ComponentManager::GetOnDraw( string_view componentName )
{
    auto iter = mComponentFunctions.find( componentName );
    if ( iter != mComponentFunctions.end() )
        return iter->second.mOnDraw;
    return nullptr;
}

void ComponentManager::AddFromJson( const string& componentName, ComponentFromJson fromJson )
{
    mComponentFunctions[componentName].mFromJson = fromJson;
}

ComponentFromJson ComponentManager::GetFromJson( string_view componentName )
{
    auto iter = mComponentFunctions.find( componentName );
    if ( iter != mComponentFunctions.end() )
        return iter->second.mFromJson;
    return nullptr;
}

void ComponentManager::AddToJson( const string& componentName, ComponentToJson toJson )
{
    mComponentFunctions[componentName].mToJson = toJson;
}

ComponentToJson ComponentManager::GetToJson( string_view componentName )
{
    auto iter = mComponentFunctions.find( componentName );
    if ( iter != mComponentFunctions.end() )
        return iter->second.mToJson;
    return nullptr;
}

}
