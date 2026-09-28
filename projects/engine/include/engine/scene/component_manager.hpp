#pragma once
#include <imgui.h>
#include <sol/forward.hpp>
#include "engine/types/json.hpp"
#include "engine/types/map.hpp"
#include "engine/loader/loader.hpp"
#include "engine/scene/scene.hpp"
#include "engine/editing/ui/inspector_context.hpp"

namespace bubble
{
template <typename Component>
concept ComponentConcept = requires( Component component,
                                     Component& componentRef,
                                     const Component& componentCRef,
                                     const Entity& entity,
                                     sol::state& lua,
                                     Project& project,
                                     InspectorContext& ctx,
                                     json& json )
{
    { Component::ID() } -> std::same_as<int>;
    { Component::Name() } -> std::same_as<string_view>;
    { Component::OnComponentDraw( ctx, entity, componentRef ) } -> std::same_as<void>;
    { Component::ToJson( json, project, componentCRef ) } -> std::same_as<void>;
    { Component::FromJson( json, project, componentRef ) } -> std::same_as<void>;
    { Component::CreateLuaBinding( lua ) } -> std::same_as<void>;
};

typedef void ( *OnComponentDrawFunc )( InspectorContext& ctx, const Entity& entity, void* rawData );
typedef void ( *ComponentToJson )( json& json, const Project& project, const void* rawData );
typedef void ( *ComponentFromJson )( const json& json, Project& project, void* rawData );
typedef void ( *ComponentCreateLuaBinding )( sol::state& lua );
// The type's storage in a registry, made if it has none yet.
typedef Scene::Storage& ( *ComponentStorageFunc )( Scene::Registry& registry );

struct ComponentFunctionsTable
{
    string_view mName;
    OnComponentDrawFunc mOnDraw = nullptr;
    ComponentFromJson mFromJson = nullptr;
    ComponentToJson mToJson = nullptr;
    ComponentCreateLuaBinding mCreateLuaBinding = nullptr;
    entt::id_type mStorageId = 0;
    ComponentStorageFunc mStorage = nullptr;
};

// Every component type the engine knows, by ComponentTypeId: its name and
// what the inspector, the file format, Lua and the id API of Scene need to
// reach it without naming the type. Filled once, before the first Scene.
class ComponentManager
{
public:
    static ComponentManager& Instance();

    template <ComponentConcept Component>
    static void Add()
    {
        Register( Component::ID(), ComponentFunctionsTable{
            .mName = Component::Name(),
            .mOnDraw = []( InspectorContext& ctx, const Entity& entity, void* rawData )
            { Component::OnComponentDraw( ctx, entity, *static_cast<Component*>( rawData ) ); },
            .mFromJson = []( const json& json, Project& project, void* rawData )
            { Component::FromJson( json, project, *static_cast<Component*>( rawData ) ); },
            .mToJson = []( json& json, const Project& project, const void* rawData )
            { Component::ToJson( json, project, *static_cast<const Component*>( rawData ) ); },
            .mCreateLuaBinding = Component::CreateLuaBinding,
            .mStorageId = entt::type_hash<Component>::value(),
            .mStorage = []( Scene::Registry& registry ) -> Scene::Storage& { return registry.storage<Component>(); },
        } );
    }

    // Every registered id, in order.
    static const vector<ComponentTypeId>& Ids();

    static string_view GetName( ComponentTypeId componentId );
    // Throws on a name no component has.
    static ComponentTypeId GetID( string_view name );
    static OnComponentDrawFunc GetOnDraw( ComponentTypeId componentId );
    static ComponentFromJson GetFromJson( ComponentTypeId componentId );
    static ComponentToJson GetToJson( ComponentTypeId componentId );
    static ComponentCreateLuaBinding GetCreateLuaBinding( ComponentTypeId componentId );
    // Throws on an id no component has.
    static const ComponentFunctionsTable& Get( ComponentTypeId componentId );

    const auto begin() { return mComponentFuncTable.begin(); }
    const auto end() { return mComponentFuncTable.end(); }

private:
    // Registering an id twice keeps the first.
    static void Register( ComponentTypeId componentId, const ComponentFunctionsTable& table );

    hash_map<ComponentTypeId, ComponentFunctionsTable> mComponentFuncTable;
    vector<ComponentTypeId> mIds;
};

}
