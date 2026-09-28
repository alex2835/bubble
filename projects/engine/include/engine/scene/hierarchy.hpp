#pragma once
#include "engine/scene/scene.hpp"
#include "engine/renderer/transform.hpp"
#include "engine/types/array.hpp"
#include "engine/types/map.hpp"
#include <optional>
#include <span>

// The scene's tree: which entity hangs under which, in what order, and where
// that puts each one in the world. The data is HierarchyComponent (parent,
// children) - on every entity - and TransformComponent (local, plus the world
// cache). Everything that changes the links goes through here, so the two
// sides of each link agree. The Entities window draws this tree; there is no
// other.
namespace bubble
{
constexpr size_t cAtEnd = size_t( -1 );

// INVALID_ENTITY for the root, and for an entity not in the tree.
Entity ParentOf( const Scene& scene, Entity entity );
std::span<const Entity> ChildrenOf( const Scene& scene, Entity entity );
// Where the entity is among its parent's children; cAtEnd if it has none.
size_t IndexInParent( const Scene& scene, Entity entity );
// Whether `ancestor` is `entity`'s parent, or its parent's, and so on.
bool IsAncestor( const Scene& scene, Entity ancestor, Entity entity );
// `entity` and everything under it, parents before children, in tree order.
vector<Entity> Subtree( const Scene& scene, Entity entity );

// Names and paths. An entity's name is its Tag's; among one parent's
// children no two are the same, so a path of names finds one entity. An
// entity without a Tag has no name and no path.
//
// Paths are names joined with '/': "props/chair" from an entity down,
// "../door" up to the parent first, "/player/camera" from the root (the root
// itself is "/"), "~/camera" from the root of the prefab instance the entity
// is in. "." is where the path starts.
string NameOf( const Scene& scene, Entity entity );
// `wanted` fit to go under `parent`: '/' taken out, an empty name (or ".",
// ".." or "~") made "Entity", and when a child other than `self` has it already,
// a number at its end counted up past theirs: chair -> chair2, chair2 -> chair3.
string UniqueChildName( const Scene& scene, Entity parent, string_view wanted, Entity self = INVALID_ENTITY );
// Renames `entity` if a sibling has its name (or the name is not a valid
// one). What every place that puts an entity under a parent calls after.
// Returns whether the name changed.
bool MakeNameUnique( Scene& scene, Entity entity );
// The same over a whole subtree, siblings in order - the first keeps its
// name. For a loaded file, which may have been edited by hand.
void MakeNamesUnique( Scene& scene, Entity top );

// An entity as a message names it: its path, '/player/camera'; one outside
// the tree by its name and id; one that is gone by its id.
string DescribeEntity( const Scene& scene, Entity entity );

// INVALID_ENTITY when nothing is there.
Entity FindByPath( const Scene& scene, Entity from, string_view path );
// Why FindByPath finds nothing, for an error: which part is missing where,
// what is there instead, and a name that looks the same but is spelled with
// other letters (a Cyrillic es for a Latin 'c'). Empty when it finds one.
string WhyPathFails( const Scene& scene, Entity from, string_view path );
// What "~" means from `entity`: the nearest of it and its ancestors that is
// a prefab instance's root - the innermost instance it is in. Outside any
// instance, the scene's root: in the Prefab Editor that is the prefab.
Entity PrefabRootOf( const Scene& scene, Entity entity );
// "/player/camera"; empty for an entity that is not under the root.
string PathOf( const Scene& scene, Entity entity );
// The path that leads from `from` to `to`: "camera", "../door", ".".
string RelativePath( const Scene& scene, Entity from, Entity to );

// The links alone - no transform changes. Attach puts an entity that hangs
// from nothing under `parent`, at `index` among its children; Detach takes
// one out and returns where it was. Both give the entity a
// HierarchyComponent if it has none.
void AttachChild( Scene& scene, Entity child, Entity parent, size_t index = cAtEnd );
size_t DetachFromParent( Scene& scene, Entity child );

// Moves `child` under `parent` (INVALID_ENTITY: under the scene's root), at
// `index`. With keepWorld it stays where it is in the world and its local
// transform changes; without, the local transform stays and it moves with
// its new parent. Refuses - returning false, changing nothing - a loop, and
// moving the root.
bool SetParent( Scene& scene, Entity child, Entity parent, bool keepWorld = true, size_t index = cAtEnd );

// A new entity with a HierarchyComponent, under `parent` (INVALID_ENTITY:
// the root).
Entity CreateChildEntity( Scene& scene, Entity parent = INVALID_ENTITY );

// A copy of `entity`'s subtree from one scene into another (or the same):
// new ids - `topId` for the top one, when given - with the links between the
// copies made to match and the top one hanging from nothing, for the caller
// to attach. `copied` is filled old -> new; State tables naming a copied
// entity are pointed at its copy.
Entity CopySubtree( Scene& from, Entity entity, Scene& to, map<Entity, Entity>& copied,
                    std::optional<size_t> topId = std::nullopt );

// State tables of the entities just copied may name entities of the scene
// they were copied from. Those that were copied along are pointed at their
// copies; the rest name nothing in this scene, and are cleared.
void RemapEntityReferences( Scene& scene, const map<Entity, Entity>& copied );

// The world matrix of an entity, worked out now by walking up its parents -
// for an edit in the middle of a frame, when the cache may be behind.
mat4 ComputeWorldMatrix( const Scene& scene, Entity entity );
// Sets the local transform so the entity lands at `world`. What the gizmo
// and physics use: both produce world placements.
void SetWorldTransform( Scene& scene, Entity entity, const Transform& world );

// Fills every TransformComponent's world cache, from the root down.
void UpdateWorldTransforms( Scene& scene );

}
