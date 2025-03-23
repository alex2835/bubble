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
template <typename Component>
concept ComponentConcept = requires( Component component,
                                     Component & componentRef,
                                     const Component & componentCRef,
                                     sol::state& lua,
                                     Loader& loader,
                                     json& json )
{
    { Component::ID() } -> std::same_as<size_t>;
    { Component::Name() } -> std::same_as<string_view>;
    { Component::OnComponentDraw( loader, componentRef ) } -> std::same_as<void>;
    { Component::ToJson( loader, json, componentCRef ) } -> std::same_as<void>;
    { Component::FromJson( loader, json, componentRef ) } -> std::same_as<void>;
    { Component::CreateLuaBinding( lua ) } -> std::same_as<void>;
};

typedef void ( *OnComponentDrawFunc )( const Loader& loader, void* rawData );
typedef void ( *ComponentToJson )( const Loader& loader, json& json, const void* rawData );
typedef void ( *ComponentFromJson )( Loader& loader, const json& json, void* rawData );
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

            AddOnDraw( Component::ID(), []( const Loader& loader, void* rawData )
            { Component::OnComponentDraw( loader, *reinterpret_cast<Component*>( rawData ) ); } );

            AddToJson( Component::ID(), []( const Loader& loader, json& json, const void* rawData )
            { Component::ToJson( loader, json, *reinterpret_cast<const Component*>( rawData ) ); } );

            AddFromJson( Component::ID(), []( Loader& loader, const json& json, void* rawData )
            { Component::FromJson( loader, json, *reinterpret_cast<Component*>( rawData ) ); } );

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
