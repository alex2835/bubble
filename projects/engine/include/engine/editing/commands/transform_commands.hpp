#pragma once
#include "engine/editing/command.hpp"
#include "engine/scene/scene.hpp"
#include "engine/renderer/transform.hpp"
#include "engine/types/map.hpp"
#include "engine/types/set.hpp"

// The viewport gizmo's steps. The inspector's transform fields go through
// property_command.hpp instead.
namespace bubble
{
// The gizmo's step: one or many transforms, from where the drag began to
// where it ended.
class TransformChangeCommand : public ICommand
{
public:
    TransformChangeCommand( Entity entity, Scene& scene, const Transform& oldTransform, const Transform& newTransform );

    string_view Name() const override { return "Transform"sv; }
    void Execute() override;
    void Undo() override;

private:
    Entity mEntity;
    Scene& mScene;
    Transform mOldTransform;
    Transform mNewTransform;
};

class MultiTransformChangeCommand : public ICommand
{
public:
    MultiTransformChangeCommand( const set<Entity>& entities,
                                 Scene& scene,
                                 const map<Entity, Transform>& oldTransforms,
                                 const map<Entity, Transform>& newTransforms );

    string_view Name() const override { return "Transform"sv; }
    void Execute() override;
    void Undo() override;

private:
    set<Entity> mEntities;
    Scene& mScene;
    map<Entity, Transform> mOldTransforms;
    map<Entity, Transform> mNewTransforms;
};

}
