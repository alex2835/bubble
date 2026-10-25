#pragma once
#include "bubble/reflection/field_path.hpp"
#include "bubble/scene/component_types.hpp"

namespace bubble
{
// A link to an entity a component holds: an EntityPath field, or an entity
// inside a Value - a script's props.
struct EntityLink
{
    Entity mHolder;
    Name mComponent;
    // Where it is in the component: "target", "waypoints[2]", "props.door".
    FieldPath mPath;
    EntityPath mLink;
    // Where it leads; Entity::Null when nowhere.
    Entity mTarget;
};

// Every link the tree holds, in the components ComponentTypes knows, and
// where each leads now. A relative link leads from its holder.
vector<EntityLink> FindLinks( const Scene& scene, const ComponentTypes& componentTypes );
}
