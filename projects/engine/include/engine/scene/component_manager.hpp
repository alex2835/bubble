#pragma once
#include <imgui.h>
#include <sol/forward.hpp>
#include "engine/types/json.hpp"
#include "engine/types/map.hpp"
#include "engine/loader/loader.hpp"
#include "engine/scene/scene.hpp"

namespace bubble
{
template <typename Component>
concept ComponentConcept = requires( Component component,
                                     Component& componentRef,
                                     const Component& componentCRef,
                                     const Entity& entity,
                                     sol::state& lua,
                                     Project& project,
                                     json& json )
{
    { Component::ID() } -> std::same_as<int>;
    { Component::Name() } -> std::same_as<string_view>;
    { Component::OnComponentDraw( project, entity, componentRef ) } -> std::same_as<void>;
    { Component::ToJson( json, project, componentCRef ) } -> std::same_as<void>;
    { Component::FromJson( json, project, componentRef ) } -> std::same_as<void>;
    { Component::CreateLuaBinding( lua ) } -> std::same_as<void>;
};

typedef void ( *OnComponentDrawFunc )( Project& project, const Entity& entity, void* rawData );
typedef void ( *ComponentToJson )( json& json, const Project& project, const void* rawData );
typedef void ( *ComponentFromJson )( const json& json, Project& project, void* rawData );
typedef void ( *ComponentCreateLuaBinding )( sol::state& lua );

struct ComponentFunctionsTable
{
    string_view mName;
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
        scene.AddComponent<Component>();
        if ( CreateComponentTable( Component::ID() ) )
        {
            AddName( Component::ID(), Component::Name() );

            AddOnDraw( Component::ID(), []( Project& project, const Entity& entity, void* rawData )
            { Component::OnComponentDraw( project, entity, *reinterpret_cast<Component*>( rawData ) ); } );

            AddToJson( Component::ID(), []( json& json, const Project& project, const void* rawData )
            { Component::ToJson( json, project, *reinterpret_cast<const Component*>( rawData ) ); } );

            AddFromJson( Component::ID(), []( const json& json, Project& project, void* rawData )
            { Component::FromJson( json, project, *reinterpret_cast<Component*>( rawData ) ); } );

            AddCreateLuaBinding( Component::ID(), Component::CreateLuaBinding );
        }
    }

    static void AddName( int componentId, string_view name );
    static string_view GetName( int componentId );
    static int GetID( string_view name );

    static void AddOnDraw( int componentId, OnComponentDrawFunc drawFunc );
    static OnComponentDrawFunc GetOnDraw( int componentId );

    static void AddFromJson( int componentId, ComponentFromJson drawFunc );
    static ComponentFromJson GetFromJson( int componentId );

    static void AddToJson( int componentId, ComponentToJson drawFunc );
    static ComponentToJson GetToJson( int componentId );

    static void AddCreateLuaBinding( int componentId, ComponentCreateLuaBinding CreateLuaBindingFunc );
    static ComponentCreateLuaBinding GetCreateLuaBinding( int componentId );

    const auto begin(){ return mComponentFuncTable.begin(); }
    const auto end() { return mComponentFuncTable.end(); }
private:
    static bool CreateComponentTable( int componentId );
    static ComponentFunctionsTable& GetComponentTable( int componentId );

    hash_map<int, ComponentFunctionsTable> mComponentFuncTable;
};

}
