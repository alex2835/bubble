#pragma once
#include "bubble/core/text.hpp"
#include "bubble/scene/entity.hpp"
#include "bubble/scene/entity_path.hpp"
#include "bubble/types/containers.hpp"
#include "bubble/types/string.hpp"
#include "bubble/types/utility.hpp"

namespace bubble
{
class Scene;

// Where an entity hangs in the scene's tree: its name, its parent, and its
// children in order. Every entity in the tree has one; the scene's root is
// the one with no parent and no name. An entity made by Scene::Create has
// none: it is outside the tree and has no path.
//
// Both sides of every link and the index by name are written by the
// functions below and nowhere else - that is what keeps them in step.
struct Hierarchy
{
    Text mName;
    Entity mParent;
    vector<Entity> mChildren;
    // The children by name - unique among siblings - so a path resolves a
    // step at a time, with no walk over the siblings, and a rename changes
    // one entry.
    hmap<string, Entity> mChildByName;
};

constexpr size_t cAtEnd = ~size_t( 0 );

// A new entity under `parent`, at `index` among its children. Its name is
// `name` made fit - a '/' becomes '_', an empty name, "." or ".." becomes
// "entity" - and unique among its siblings: chair, chair_2, chair_3. A
// parent outside the tree is a bug, a logic_error.
Entity CreateChild( Scene& scene, Entity parent, const Text& name, size_t index = cAtEnd );

// Entity::Null for the root and for an entity outside the tree.
Entity ParentOf( const Scene& scene, Entity entity );
// In order. Good until the tree changes.
span<const Entity> ChildrenOf( const Scene& scene, Entity entity );
// Empty for the root and for an entity outside the tree.
Text NameOf( const Scene& scene, Entity entity );
bool InTree( const Scene& scene, Entity entity );
// Whether `ancestor` is `entity`'s parent, or its parent's, and so on.
bool IsAncestor( const Scene& scene, Entity ancestor, Entity entity );

// Gives `entity` a new name, made fit and unique as CreateChild makes it;
// returns the name it got. The root has no name to change, and an entity
// outside the tree has no siblings to be unique among: both are a
// logic_error.
Text Rename( Scene& scene, Entity entity, const Text& wanted );

// Moves `child` with everything under it beneath `parent`, at `index` among
// the children `parent` has without it - so within one parent it is a
// change of place. Its name is made unique there. Refuses, changing
// nothing, to move the root or an entity outside the tree, and to put an
// entity inside itself.
expected<void, string> TrySetParent( Scene& scene, Entity child, Entity parent, size_t index = cAtEnd );

// "/props/chair"; the root's is "/"; empty outside the tree.
EntityPath PathOf( const Scene& scene, Entity entity );
// The way from `from` to `to`: "chair", "../door", "."; empty when either
// is outside the tree.
EntityPath RelativePath( const Scene& scene, Entity from, Entity to );
// The entity a path names: an absolute one from the root, a relative one
// from `from`. The error says where the path stopped and what is there:
// "/props/chiar: /props has no child 'chiar' (children: 'chair', 'table')".
expected<Entity, string> TryResolve( const Scene& scene, const EntityPath& path, Entity from = Entity::Null );

// An entity as a message names it: its path; "entity 7 (not in the tree)",
// "entity 7 (gone)", "no entity".
string Describe( const Scene& scene, Entity entity );
}
