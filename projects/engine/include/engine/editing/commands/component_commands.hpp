#pragma once
#include "engine/editing/command.hpp"
#include "engine/scene/scene.hpp"
#include "engine/types/string.hpp"

// The component set of an entity.
namespace bubble
{
class Project;

// Add a component by id, with the defaults that component starts with in the
// inspector (a State component gets a table from the project's VM).
class AddComponentCommand : public ICommand
{
public:
    AddComponentCommand( Entity entity, ComponentTypeId componentId, Project& project );

    string_view Name() const override { return mName; }
    void Execute() override;
    void Undo() override;

private:
    Entity mEntity;
    ComponentTypeId mComponentId;
    Project& mProject;
    string mName;
};

// Remove a component; the removed one waits in a private scene for undo.
class RemoveComponentCommand : public ICommand
{
public:
    RemoveComponentCommand( Entity entity, ComponentTypeId componentId, Scene& scene );

    string_view Name() const override { return mName; }
    void Execute() override;
    void Undo() override;

private:
    Entity mEntity;
    ComponentTypeId mComponentId;
    Scene& mScene;
    Scene mBackupScene;
    Entity mBackupEntity = INVALID_ENTITY;
    string mName;
};

}
