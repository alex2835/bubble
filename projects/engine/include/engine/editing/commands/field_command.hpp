#pragma once
#include "engine/editing/command.hpp"
#include "engine/scene/scene.hpp"
#include <entt/meta/meta.hpp>

namespace bubble
{
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
