#pragma once
#include "engine/editing/command.hpp"
#include "engine/project/project_tree.hpp"
#include "engine/scene/scene.hpp"
#include "engine/renderer/transform.hpp"
#include "engine/types/map.hpp"
#include "engine/types/set.hpp"

// The structural edits of a level: nodes of the tree and the entities they
// stand for, and the component set of an entity. Property edits are
// property_command.hpp.
namespace bubble
{
class Project;

// A deleted entity is not gone while the command lives: it is parked in a
// private scene under its own id, and comes back under that id on undo, so
// every later command that names it still means the same thing.
class DeleteNodeCommand : public ICommand
{
public:
    DeleteNodeCommand( Ref<ProjectTreeNode> node, Scene& scene );

    string_view Name() const override { return "Delete node"sv; }
    void Execute() override;
    void Undo() override;

private:
    void RestoreNodeEntities( Ref<ProjectTreeNode>& node );

    Ref<ProjectTreeNode> mNode;
    Ref<ProjectTreeNode> mParent;
    Scene& mScene;
    Scene mBackupScene;
    map<Entity, Entity> mEntityMapping; // main scene entity -> backup scene entity
    size_t mIndexInParent = 0;
};

class DeleteMultipleNodesCommand : public ICommand
{
public:
    DeleteMultipleNodesCommand( const vector<Ref<ProjectTreeNode>>& nodes, Scene& scene );

    string_view Name() const override { return "Delete nodes"sv; }
    void Execute() override;
    void Undo() override;

private:
    void RestoreNodeEntities( Ref<ProjectTreeNode>& node );

    struct NodeInfo
    {
        Ref<ProjectTreeNode> mNode;
        Ref<ProjectTreeNode> mParent;
        size_t mIndexInParent = 0;
    };

    vector<NodeInfo> mNodeInfos;
    Scene& mScene;
    Scene mBackupScene;
    map<Entity, Entity> mEntityMapping;
};

// The copy is made once; undo parks its entities and redo brings them
// back under their ids, like a delete in reverse.
class CopyNodeCommand : public ICommand
{
public:
    CopyNodeCommand( Ref<ProjectTreeNode> sourceNode, Ref<ProjectTreeNode> targetParent, Scene& scene );

    string_view Name() const override { return "Copy node"sv; }
    void Execute() override;
    void Undo() override;
    void Redo() override;

private:
    Ref<ProjectTreeNode> mSourceNode;
    Ref<ProjectTreeNode> mTargetParent;
    Ref<ProjectTreeNode> mCopiedNode;
    Scene& mScene;
    Scene mBackupScene;
    map<Entity, Entity> mEntityMapping;
};

// A new node under `parent`, of one of the kinds the tree knows. For the
// entity kinds the command also makes the entity, with the components that
// kind starts with, at `spawnAt`; a Folder makes none. The recipe lives here
// so that creating an object is one step to undo, not a node on top of an
// entity that was made elsewhere.
class CreateNodeCommand : public ICommand
{
public:
    CreateNodeCommand( Ref<ProjectTreeNode> parent,
                       ProjectTreeNodeType type,
                       Project& project,
                       const Transform& spawnAt );

    string_view Name() const override { return mName; }
    void Execute() override; // makes the node and its entity
    void Undo() override;    // parks the entity
    void Redo() override;    // brings it back under the same id
    ~CreateNodeCommand() override;

    Ref<ProjectTreeNode> GetCreatedNode() const { return mCreatedNode; }

    // What each kind starts with. Public because it is the one place that
    // knows, and the Lua bindings may want the same defaults.
    static Entity CreateEntityFor( ProjectTreeNodeType type, Project& project, const Transform& spawnAt );

private:
    Ref<ProjectTreeNode> mParent;
    Ref<ProjectTreeNode> mCreatedNode;
    ProjectTreeNodeType mType;
    Project& mProject;
    Transform mSpawnAt;
    string mName;
    Scene mBackupScene;
    Entity mBackupEntity = INVALID_ENTITY;
};

class MoveNodeCommand : public ICommand
{
public:
    MoveNodeCommand( Ref<ProjectTreeNode> node, Ref<ProjectTreeNode> newParent );

    string_view Name() const override { return "Move node"sv; }
    void Execute() override;
    void Undo() override;

private:
    Ref<ProjectTreeNode> mNode;
    Ref<ProjectTreeNode> mOldParent;
    Ref<ProjectTreeNode> mNewParent;
    size_t mOldIndexInParent = 0;
};

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
