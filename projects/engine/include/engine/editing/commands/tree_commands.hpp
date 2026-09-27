#pragma once
#include "engine/editing/command.hpp"
#include "engine/project/project_tree.hpp"
#include "engine/scene/scene.hpp"
#include "engine/renderer/transform.hpp"
#include "engine/project/prefab.hpp"
#include "engine/types/map.hpp"
#include "engine/types/set.hpp"

// The structure of a level: nodes of the tree and the entities they stand
// for - made, deleted, copied, moved.
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
// back under their ids, like a delete in reverse. Pasted under another
// parent, the copy stays where the original is in the world.
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
                       Level& level,
                       const Transform& spawnAt );

    string_view Name() const override { return mName; }
    void Execute() override; // makes the node and its entity
    void Undo() override;    // parks the entity
    void Redo() override;    // brings it back under the same id
    ~CreateNodeCommand() override;

    Ref<ProjectTreeNode> GetCreatedNode() const { return mCreatedNode; }

    // What each kind starts with. Public because it is the one place that
    // knows, and the Lua bindings may want the same defaults.
    static Entity CreateEntityFor( ProjectTreeNodeType type, Project& project, Scene& scene, const Transform& spawnAt );

private:
    Ref<ProjectTreeNode> mParent;
    Ref<ProjectTreeNode> mCreatedNode;
    ProjectTreeNodeType mType;
    Project& mProject;
    Level& mLevel;
    Transform mSpawnAt;
    string mName;
    Scene mBackupScene;
    Entity mBackupEntity = INVALID_ENTITY;
};

// Moves a node under another - which, for an entity under an entity node,
// is parenting it. The moved entities stay where they are in the world:
// their local transforms are worked out again against the new parent.
class MoveNodeCommand : public ICommand
{
public:
    MoveNodeCommand( Ref<ProjectTreeNode> node, Ref<ProjectTreeNode> newParent, Scene& scene );

    string_view Name() const override { return "Move node"sv; }
    void Execute() override;
    void Undo() override;

private:
    Ref<ProjectTreeNode> mNode;
    Ref<ProjectTreeNode> mOldParent;
    Ref<ProjectTreeNode> mNewParent;
    Scene& mScene;
    size_t mOldIndexInParent = 0;
    // The local transforms the move replaced, for undo.
    map<Entity, Transform> mOldLocals;
};

// A prefab instantiated under `parent` - see prefab.hpp. Undo parks the
// instance, redo brings it back under the same ids, like a paste.
class InstantiatePrefabCommand : public ICommand
{
public:
    // `index` in the parent's children, the end by default; `rootId` makes
    // the root under a given id, which is how an update keeps the one the
    // old instance had.
    InstantiatePrefabCommand( Project& project,
                              Level& level,
                              Ref<ProjectTreeNode> parent,
                              path relPrefab,
                              PrefabPlacement placement,
                              size_t index = size_t( -1 ),
                              std::optional<size_t> rootId = std::nullopt );

    string_view Name() const override { return "Instantiate prefab"sv; }
    void Execute() override;
    void Undo() override;
    void Redo() override;

    Ref<ProjectTreeNode> GetRoot() const { return mRoot; }

private:
    Project& mProject;
    Level& mLevel;
    Ref<ProjectTreeNode> mParent;
    path mPrefab;
    PrefabPlacement mPlacement;
    size_t mIndex;
    std::optional<size_t> mRootId;
    Ref<ProjectTreeNode> mRoot;
    Scene mBackupScene;
    map<Entity, Entity> mEntityMapping;
};

// An instance made again from its prefab as it is now: the old one deleted,
// a new one put in its place, its root under the same id, where it was and
// turned as it was - so what names the instance still does. The rest comes
// from the prefab; what was changed under it by hand is replaced.
Command MakeRefreshPrefabInstance( Project& project, Level& level, const Ref<ProjectTreeNode>& instance );

// Whether `node` is `ancestor` or somewhere under it: what a node may not be
// moved into.
bool IsInSubtree( const Ref<ProjectTreeNode>& node, const Ref<ProjectTreeNode>& ancestor );

}
