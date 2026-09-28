#pragma once
#include "engine/project/level.hpp"
#include "engine/renderer/transform.hpp"
#include "engine/types/array.hpp"
#include <optional>

// Prefabs: a group of entities saved to a file of its own and placed into
// levels as many times as wanted.
//
// A .prefab file is a level file - the same scene, the same serialization -
// so the prefab editor opens one the way the editor opens a level, as a
// document of its own. Its root is the prefab: an instance is a copy of that
// root and everything under it, with a PrefabInstanceComponent naming the
// file put on the copy of the root.
namespace bubble
{
class Project;

constexpr string_view PREFAB_FILE_EXT = ".prefab"sv;

// Where an instance's root goes.
struct PrefabPlacement
{
    // Spawned: at this point in the world, turned and scaled as the prefab's
    // root is.
    std::optional<vec3> mWorldPosition;
    // Refreshed: where the old root was, and how it was turned - those are
    // the instance's. Its scale is the prefab's, so a change of it in the
    // prefab reaches every instance.
    std::optional<Transform> mLocal;
    // Refreshed: the old root's name, which is the instance's too.
    std::optional<string> mName;
};

// `relPrefab` (relative to the project root) instantiated into `scene` under
// `parent`, as its `index`th child: the instance's root. With `rootId` the
// root is made under that id. Throws if the file cannot be read.
Entity InstantiatePrefab( Project& project,
                          Scene& scene,
                          Entity parent,
                          size_t index,
                          const path& relPrefab,
                          const PrefabPlacement& placement,
                          Entity rootId = Entity::Null );

// Writes `entity` and everything under it as a prefab, `entity` its root:
// placed at the origin, turned and scaled as it is in the world.
void SavePrefab( Scene& scene, Entity entity, const path& absFile, Project& project );

// The instance roots of `relPrefab` in the scene, in tree order; of every
// prefab when it is empty. Not looked for inside an instance of the same
// prefab (which cannot contain itself).
vector<Entity> FindPrefabInstances( const Scene& scene, const string& relPrefab );

// A prefab spawned into a running scene, under its root, at `position`.
// Returns every entity made, the instance's root first - the caller wires
// physics and scripts up for them.
vector<Entity> SpawnPrefab( Project& project, Scene& scene, const path& relPrefab, const vec3& position );

}
