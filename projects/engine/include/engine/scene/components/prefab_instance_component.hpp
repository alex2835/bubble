#pragma once
#include "engine/scene/components/component_base.hpp"

namespace bubble
{
// Marks the root of a prefab instance: the entity everything instantiated
// from a .prefab hangs under, and the file it came from (relative to the
// project root). Updating the instances of a prefab finds them by this.
//
// Remove it and the instance is unpacked: plain entities from then on, which
// an update of the prefab no longer touches.
struct PrefabInstanceComponent
{
    static int ID() { return static_cast<int>( ComponentID::PrefabInstance ); }
    static string_view Name() { return "PrefabInstance"sv; }

    static void OnComponentDraw( InspectorContext& ctx, const Entity& entity, PrefabInstanceComponent& component );
    static void ToJson( json& json, const Project& project, const PrefabInstanceComponent& component );
    static void FromJson( const json& json, Project& project, PrefabInstanceComponent& component );
    static void CreateLuaBinding( sol::state& lua );

    PrefabInstanceComponent() = default;
    explicit PrefabInstanceComponent( string prefab ) : mPrefab( std::move( prefab ) ) {}

    string mPrefab;
};

}
