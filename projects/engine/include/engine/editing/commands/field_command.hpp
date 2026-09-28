#pragma once
#include "engine/editing/command.hpp"
#include "engine/scene/scene.hpp"
#include <entt/meta/meta.hpp>

namespace bubble
{
// A field of a component as the editor names it: "Light.brightness",
// "Transform.position" - the component's name, then a path into it.
struct ComponentField
{
    ComponentTypeId mComponentId;
    string mPath; // within the component: "brightness", "points[2].value"
};

// Throws when there is no component name before the first dot, or no
// component by that name.
ComponentField ParseComponentField( string_view path );

// The component, by reference, as engine/reflection sees it. Throws saying
// why not: the entity is not there, has no such component, or the component
// does not describe its fields yet.
entt::meta_any RequireReflected( Scene& scene, Entity entity, ComponentTypeId componentId );

// A field of a reflected component set from one value to another, the field
// named by a path (engine/reflection): "brightness", "points[2].value". The
// component is looked up again each time, by entity and type - it moves in
// its storage, and the entity may have been deleted and restored in between.
class SetFieldCommand : public ICommand
{
public:
    SetFieldCommand( Scene& scene, Entity entity, ComponentTypeId componentId, string path,
                     entt::meta_any from, entt::meta_any to );

    string_view Name() const override { return mName; }
    void Execute() override { Set( mTo ); }
    void Undo() override { Set( mFrom ); }

private:
    void Set( const entt::meta_any& value );

    Scene& mScene;
    Entity mEntity;
    ComponentTypeId mComponentId;
    string mPath;
    string mName;
    entt::meta_any mFrom;
    entt::meta_any mTo;
};

}
