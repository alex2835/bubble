#pragma once
#include <imgui.h>
#include <sol/forward.hpp>
#include "engine/types/json.hpp"
#include "engine/types/map.hpp"
#include "engine/loader/loader.hpp"
#include "engine/scene/scene.hpp"
#include "engine/editing/ui/inspector_context.hpp"
#include "engine/reflection/reflection.hpp"
#include "engine/editing/ui/reflected_inspector.hpp"

namespace bubble
{
// A component describes its fields for engine/reflection - Reflect() -
// and is saved from that description; one that does not yet writes its own
// ToJson and FromJson.
template <typename Component>
concept ReflectedComponent = requires { { Component::Reflect() } -> std::same_as<void>; };

template <typename Component>
concept HandSavedComponent = requires( Component& componentRef,
                                       const Component& componentCRef,
                                       Project& project,
                                       json& json )
{
    { Component::ToJson( json, project, componentCRef ) } -> std::same_as<void>;
    { Component::FromJson( json, project, componentRef ) } -> std::same_as<void>;
};

// Drawn in the inspector by code of its own. A reflected component without
// it is drawn from its description (reflected_inspector.hpp).
template <typename Component>
concept HandDrawnComponent = requires( Component& componentRef, const Entity& entity, InspectorContext& ctx )
{
    { Component::OnComponentDraw( ctx, entity, componentRef ) } -> std::same_as<void>;
};

template <typename Component>
concept ComponentConcept = requires( sol::state& lua )
{
    { Component::ID() } -> std::same_as<int>;
    { Component::Name() } -> std::same_as<string_view>;
    { Component::CreateLuaBinding( lua ) } -> std::same_as<void>;
} and ( ReflectedComponent<Component> or HandSavedComponent<Component> )
  and ( ReflectedComponent<Component> or HandDrawnComponent<Component> );

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
    // Its description, for a reflected component; empty otherwise.
    entt::meta_type mMeta;
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
        ComponentFunctionsTable table{
            .mName = Component::Name(),
            .mCreateLuaBinding = Component::CreateLuaBinding,
            .mStorageId = entt::type_hash<Component>::value(),
            .mStorage = []( Scene::Registry& registry ) -> Scene::Storage& { return registry.storage<Component>(); },
        };
        if constexpr ( HandDrawnComponent<Component> )
            table.mOnDraw = []( InspectorContext& ctx, const Entity& entity, void* rawData )
            { Component::OnComponentDraw( ctx, entity, *static_cast<Component*>( rawData ) ); };
        else
            table.mOnDraw = []( InspectorContext& ctx, const Entity& entity, void* )
            { DrawComponentFields( ctx, entity, Component::ID() ); };

        if constexpr ( ReflectedComponent<Component> )
        {
            Component::Reflect();
            table.mMeta = entt::resolve<Component>();
            table.mFromJson = []( const json& json, Project&, void* rawData )
            {
                entt::meta_any component = entt::forward_as_meta( *static_cast<Component*>( rawData ) );
                bubble::FromJson( json, component );
            };
            table.mToJson = []( json& json, const Project&, const void* rawData )
            { json = bubble::ToJson( entt::forward_as_meta( *static_cast<const Component*>( rawData ) ) ); };
        }
        else
        {
            table.mFromJson = []( const json& json, Project& project, void* rawData )
            { Component::FromJson( json, project, *static_cast<Component*>( rawData ) ); };
            table.mToJson = []( json& json, const Project& project, const void* rawData )
            { Component::ToJson( json, project, *static_cast<const Component*>( rawData ) ); };
        }
        Register( Component::ID(), table );
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
    // The component, by reference, as engine/reflection sees it; empty when
    // the entity does not have it or the type is not reflected.
    static entt::meta_any Reflected( Scene& scene, Entity entity, ComponentTypeId componentId );

    const auto begin() { return mComponentFuncTable.begin(); }
    const auto end() { return mComponentFuncTable.end(); }

private:
    // Registering an id twice keeps the first.
    static void Register( ComponentTypeId componentId, const ComponentFunctionsTable& table );

    hash_map<ComponentTypeId, ComponentFunctionsTable> mComponentFuncTable;
    vector<ComponentTypeId> mIds;
};

}
