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
