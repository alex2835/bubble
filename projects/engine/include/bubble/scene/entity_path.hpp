#pragma once
#include "bubble/core/text.hpp"

namespace bubble
{
// A link to an entity as a file keeps it: its path in the tree, absolute
// from the root ("/props/chair") or from the entity that holds the link
// ("../door"). Names of entities are Text, so the path is too. TryResolve in
// bubble/scene/hierarchy.hpp finds the entity.
struct EntityPath
{
    Text mPath;

    friend bool operator==( const EntityPath&, const EntityPath& ) = default;
};
}
