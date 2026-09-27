#pragma once
#include "engine/scene/scene.hpp"
#include "engine/types/string.hpp"

// A reference to an entity by where it is in the tree - "../door",
// "camera", "/player" - rather than by its id. Kept in a State table, it is
// relative to the entity that owns the table, so a prefab can name its own
// parts: the path stays right in every instance and across an update, where
// the ids change. Paths are those of engine/scene/hierarchy.hpp.
//
// In the editor it stays a path. When the game starts, and when something is
// spawned, every NodePath in a State table is replaced by the entity it leads
// to (nil when it leads nowhere), so scripts get entities.
namespace bubble
{
struct NodePath
{
    NodePath() = default;
    explicit NodePath( string path ) : mPath( std::move( path ) ) {}
    bool operator==( const NodePath& ) const = default;

    string mPath;
};

// Every NodePath in `owner`'s State, at any depth, turned into the entity it
// leads to from `owner`. Paths that lead nowhere become nil, and are logged.
void ResolveNodePaths( Scene& scene, Entity owner );
// The same for every entity with a State.
void ResolveAllNodePaths( Scene& scene );

}
