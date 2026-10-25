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

// Every link `root` and what is under it hold, in the components
// ComponentTypes knows, and where each leads now. A relative link leads
// from its holder.
vector<EntityLink> FindLinks( const Scene& scene, const ComponentTypes& componentTypes, Entity root );

// `link` moved from under `from` to under `to`: "/props/chair/leg" from
// "/props/chair" to "/props/chair_2" is "/props/chair_2/leg". Nothing when
// it is not under `from` - "/props/chairs" is not under "/props/chair".
opt<EntityPath> TryRebase( const EntityPath& link, const EntityPath& from, const EntityPath& to );
}
