#pragma once
#include "engine/scene/scene.hpp"
#include "engine/renderer/transform.hpp"
#include "engine/types/pointer.hpp"
#include <span>

// The scene hierarchy: which entity hangs under which, and where that puts
// each one in the world. The data is HierarchyComponent (parent, children)
// and TransformComponent (local, plus the world cache); everything that
// changes the links goes through here, so the two sides of each link agree.
//
// In the editor the level's tree is where the links are made: an entity node
// under another entity's node - through any folders between them - is that
// entity's child, and SyncHierarchy writes the tree into the components. At
// run time there is no tree to speak of; SetParent is the way.
namespace bubble
{
struct ProjectTreeNode;

// INVALID_ENTITY for a root.
Entity ParentOf( const Scene& scene, Entity entity );
std::span<const Entity> ChildrenOf( const Scene& scene, Entity entity );
// Whether `ancestor` is `entity`'s parent, or its parent's, and so on.
bool IsAncestor( const Scene& scene, Entity ancestor, Entity entity );

// Hangs `child` under `parent` (INVALID_ENTITY makes it a root), last among
// the parent's children. With keepWorld it stays where it is in the world and
// its local transform changes; without, the local transform stays and the
// entity moves with its new parent. Refuses - returning false, changing
// nothing - a link that would make a loop.
bool SetParent( Scene& scene, Entity child, Entity parent, bool keepWorld = true );

// The world matrix of an entity, worked out now by walking up its parents -
// for an edit in the middle of a frame, when the cache may be behind.
mat4 ComputeWorldMatrix( const Scene& scene, Entity entity );
// Sets the local transform so the entity lands at `world`. What the gizmo
// and physics use: both produce world placements.
void SetWorldTransform( Scene& scene, Entity entity, const Transform& world );

// Fills every TransformComponent's world cache, from the roots down.
void UpdateWorldTransforms( Scene& scene );

// Writes the tree's structure into the HierarchyComponents of the entities
// in it: each entity's parent is the nearest entity node above it, folders
// being see-through, and its children are in tree order. An entity that ends
// up with neither loses its component. Cheap enough to run every frame the
// tree is being edited.
void SyncHierarchy( Scene& scene, const Ref<ProjectTreeNode>& root );

}
