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
    static string_view Name() { return "prefab_instance"sv; }

    // Fields for engine/reflection: saved, shown, set by path and bound to Lua.
    static void Reflect();

    PrefabInstanceComponent() = default;
    explicit PrefabInstanceComponent( string prefab ) : mPrefab( std::move( prefab ) ) {}

    string mPrefab;
};

}
