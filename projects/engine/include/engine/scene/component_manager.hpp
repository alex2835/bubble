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
    { Component::ID() } -> std::same_as<size_t>;
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

    static void AddName( size_t size_t, string_view name );
    static string_view GetName( size_t size_t );
    static size_t GetID( string_view name );

    static void AddOnDraw( size_t size_t, OnComponentDrawFunc drawFunc );
    static OnComponentDrawFunc GetOnDraw( size_t size_t );

    static void AddFromJson( size_t size_t, ComponentFromJson drawFunc );
    static ComponentFromJson GetFromJson( size_t size_t );

    static void AddToJson( size_t size_t, ComponentToJson drawFunc );
    static ComponentToJson GetToJson( size_t size_t );

    static void AddCreateLuaBinding( size_t size_t, ComponentCreateLuaBinding CreateLuaBindingFunc );
    static ComponentCreateLuaBinding GetCreateLuaBinding( size_t size_t );

    const auto begin(){ return mComponentFuncTable.begin(); }
    const auto end() { return mComponentFuncTable.end(); }
private:
    static bool CreateComponentTable( size_t size_t );
    static ComponentFunctionsTable& GetComponentTable( size_t size_t );

    hash_map<size_t, ComponentFunctionsTable> mComponentFuncTable;
};

}
