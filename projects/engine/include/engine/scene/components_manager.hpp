#pragma once
#include <imgui.h>
#include <sol/forward.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/euler_angles.hpp>
#include "engine/types/json.hpp"
#include "engine/types/map.hpp"
#include "engine/loader/loader.hpp"
#include "engine/scene/scene.hpp"

namespace bubble
{
template <typename ComponentType>
concept ComponentConcept = requires( ComponentType component,
                                     ComponentType& componentRef,
                                     const ComponentType& componentCRef,
                                     sol::state& lua,
                                     Loader& loader,
                                     json& json )
{
    { ComponentType::Name() } -> std::same_as<string_view>;
    { ComponentType::OnComponentDraw( loader, componentRef ) } -> std::same_as<void>;
    { ComponentType::ToJson( loader, json, componentCRef ) } -> std::same_as<void>;
    { ComponentType::FromJson( loader, json, componentRef ) } -> std::same_as<void>;
    { ComponentType::CreateLuaBinding( lua ) } -> std::same_as<void>;
};

typedef void ( *OnComponentDrawFunc )( const Loader& loader, void* rawData );
typedef void ( *ComponentToJson )( const Loader& loader, json& json, const void* rawData );
typedef void ( *ComponentFromJson )( Loader& loader, const json& json, void* rawData );
typedef void ( *ComponentCreateLuaBinding )( sol::state& lua );

struct ComponentFunctionsTable
{
    OnComponentDrawFunc mOnDraw;
    ComponentFromJson mFromJson;
    ComponentToJson mToJson;
    ComponentCreateLuaBinding mCreateLuaBinding;
};


class ComponentManager
{
public:
    static ComponentManager& Instance();

    template <ComponentConcept Component>
    static void Add( Scene& scene )
    {
        scene.AddComponet<Component>();
        CreateComponentTable( Component::Name() );

        AddOnDraw( Component::Name(), []( const Loader& loader, void* rawData )
        { Component::OnComponentDraw( loader, *reinterpret_cast<Component*>( rawData ) ); } );

        AddToJson( Component::Name(), []( const Loader& loader, json& json, const void* rawData )
        { Component::ToJson( loader, json, *reinterpret_cast<const Component*>( rawData ) ); } );

        AddFromJson( Component::Name(), []( Loader& loader, const json& json, void* rawData ) 
        { Component::FromJson( loader, json, *reinterpret_cast<Component*>( rawData ) ); } );

        AddCreateLuaBinding( Component::Name(), Component::CreateLuaBinding );
    }

    static void AddOnDraw( string_view componentName, OnComponentDrawFunc drawFunc );
    static OnComponentDrawFunc GetOnDraw( string_view componentName );

    static void AddFromJson( string_view componentName, ComponentFromJson drawFunc );
    static ComponentFromJson GetFromJson( string_view componentName );

    static void AddToJson( string_view componentName, ComponentToJson drawFunc );
    static ComponentToJson GetToJson( string_view componentName );

    static void AddCreateLuaBinding( string_view componentName, ComponentCreateLuaBinding CreateLuaBindingFunc );
    static ComponentCreateLuaBinding GetCreateLuaBinding( string_view componentName );

    const auto begin(){ return mComponentFuncTable.begin(); }
    const auto end() { return mComponentFuncTable.end(); }
private:
    static void CreateComponentTable( string_view componentName );
    static ComponentFunctionsTable& GetComponentTable( string_view componentName );

    str_hash_map<ComponentFunctionsTable> mComponentFuncTable;
};

}
