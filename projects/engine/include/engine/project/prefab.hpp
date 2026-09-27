#pragma once
#include "engine/project/level.hpp"
#include "engine/renderer/transform.hpp"
#include "engine/types/map.hpp"
#include <optional>

// Prefabs: a group of entities saved to a file of its own and placed into
// levels as many times as wanted.
//
// A .prefab file is a level file - the same scene and tree, the same
// serialization - so the prefab editor opens one the way the editor opens a
// level, as a document of its own. What makes it a prefab is how it is used:
// its content is copied into a level under an instance root, an entity with a
// PrefabInstanceComponent naming the file. When the prefab has exactly one
// entity at the top, that entity is the root; otherwise one is made to hold
// them.
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
};

// A copy of `node`'s subtree from `from` into `to`: new tree nodes, numbered
// by `to`'s counter, and new entities, noted in `copied` (old -> new). With
// `rootId` the top entity is made under that id. The copy is not attached to
// anything.
Ref<ProjectTreeNode> CopySubtreeInto( const Ref<ProjectTreeNode>& node,
                                      Scene& from,
                                      Level& to,
                                      map<Entity, Entity>& copied,
                                      std::optional<size_t> rootId = std::nullopt );

// State tables of the entities just copied may name entities of the scene
// they were copied from. Those that were copied along are pointed at their
// copies; the rest name nothing in this scene, and are cleared.
void RemapEntityReferences( Scene& scene, const map<Entity, Entity>& copied );

// `relPrefab` (relative to the project root) instantiated into `level`
// under `parent`, as its `index`th child: the instance's root node. Throws
// if the file cannot be read.
Ref<ProjectTreeNode> InstantiatePrefab( Project& project,
                                        Level& level,
                                        const Ref<ProjectTreeNode>& parent,
                                        size_t index,
                                        const path& relPrefab,
                                        const PrefabPlacement& placement,
                                        std::optional<size_t> rootId = std::nullopt );

// Writes `node` and everything under it as a prefab: its entities moved so
// the first of them sits at the origin, keeping how they are turned and
// scaled in the world.
void SavePrefab( const Ref<ProjectTreeNode>& node, Scene& scene, const path& absFile, Project& project );

// The instance roots of `relPrefab` in the level, in tree order; of every
// prefab when it is empty. Not looked for inside an instance of the same
// prefab (which cannot contain itself).
vector<Ref<ProjectTreeNode>> FindPrefabInstances( const Level& level, const string& relPrefab );

// A prefab's content spawned into a running scene, with no tree: the same
// entities and links, placed at `position`. Returns every entity made, the
// root first - the caller wires physics and scripts up for them.
vector<Entity> SpawnPrefab( Project& project, Scene& scene, const path& relPrefab, const vec3& position );

}
