#pragma once
#include "engine/editing/command.hpp"
#include "engine/scene/scene.hpp"
#include "engine/renderer/transform.hpp"
#include "engine/project/prefab.hpp"
#include "engine/types/array.hpp"
#include "engine/types/map.hpp"

// The structure of a scene: entities made, deleted, copied, moved in its
// tree (engine/scene/hierarchy.hpp).
//
// An entity taken out is not gone while the command lives: it is parked,
// with everything under it, in a private scene under its own id, and comes
// back under that id - so every later command that names it still means the
// same thing.
namespace bubble
{
class Project;
class Level;

// What the editor's menu makes: each kind is the components it starts with.
enum class EntityKind
{
    Folder,
    ModelObject,
    PhysicsObject,
    GameObject,
    Camera,
    Script,
    Light,
    Audio,
};

class CreateEntityCommand : public ICommand
{
public:
    // Made under `parent`, last; at `spawnAt` in the world (a folder: where
    // its parent is).
    CreateEntityCommand( Project& project, Scene& scene, Entity parent, EntityKind kind, const Transform& spawnAt );

    string_view Name() const override { return mName; }
    void Execute() override;
    void Undo() override;
    void Redo() override;

    Entity Created() const { return mEntity; }

    // The components each kind starts with, on a new entity that hangs from
    // nothing yet. The one place that knows.
    static Entity MakeEntity( EntityKind kind, Project& project, Scene& scene, const Transform& spawnAt );

private:
    Project& mProject;
    Scene& mScene;
    Entity mParent;
    EntityKind mKind;
    Transform mSpawnAt;
    string mName;
    Entity mEntity = INVALID_ENTITY;
    size_t mIndex = 0;
    Scene mBackup;
};

// Deletes entities and what is under them. One under another that is
// deleted too goes with it; the root is never deleted.
class DeleteEntitiesCommand : public ICommand
{
public:
    DeleteEntitiesCommand( Scene& scene, vector<Entity> entities );

    string_view Name() const override { return "Delete"sv; }
    void Execute() override;
    void Undo() override;

private:
    struct Taken
    {
        Entity mEntity;
        Entity mParent;
        size_t mIndex;
    };
    Scene& mScene;
    vector<Entity> mEntities;
    vector<Taken> mTaken;
    Scene mBackup;
};

// A copy of an entity and what is under it, under `parent`, last. Pasted
// under another parent, it stays where the original is in the world.
class CopyEntityCommand : public ICommand
{
public:
    CopyEntityCommand( Scene& scene, Entity source, Entity parent );

    string_view Name() const override { return "Copy"sv; }
    void Execute() override;
    void Undo() override;
    void Redo() override;

    Entity Copy() const { return mCopy; }

private:
    Scene& mScene;
    Entity mSource;
    Entity mParent;
    Entity mCopy = INVALID_ENTITY;
    size_t mIndex = 0;
    Scene mBackup;
};

// Moves an entity under another, at `index` among its children - which is
// parenting it. It stays where it is in the world: its local transform is
// worked out again against the new parent. A name one of the new siblings
// has gets a number.
class MoveEntityCommand : public ICommand
{
public:
    MoveEntityCommand( Scene& scene, Entity entity, Entity parent, size_t index = size_t( -1 ) );

    string_view Name() const override { return "Move"sv; }
    void Execute() override;
    void Undo() override;

private:
    Scene& mScene;
    Entity mEntity;
    Entity mParent;
    size_t mIndex;
    Entity mOldParent = INVALID_ENTITY;
    size_t mOldIndex = 0;
    Transform mOldLocal;
    string mOldName;
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
                              Scene& scene,
                              Entity parent,
                              path relPrefab,
                              PrefabPlacement placement,
                              size_t index = size_t( -1 ),
                              std::optional<size_t> rootId = std::nullopt );

    string_view Name() const override { return "Instantiate prefab"sv; }
    void Execute() override;
    void Undo() override;
    void Redo() override;

    Entity Root() const { return mRoot; }

private:
    Project& mProject;
    Scene& mScene;
    Entity mParent;
    path mPrefab;
    PrefabPlacement mPlacement;
    size_t mIndex;
    std::optional<size_t> mRootId;
    Entity mRoot = INVALID_ENTITY;
    Scene mBackup;
};

// An instance made again from its prefab as it is now: the old one deleted,
// a new one put in its place, its root under the same id, where it was and
// turned as it was - so what names the instance still does. The rest comes
// from the prefab; what was changed under it by hand is replaced.
Command MakeRefreshPrefabInstance( Project& project, Scene& scene, Entity instance );

// Renames an entity, to `wanted` or - when a sibling has that - to it with a
// number. Null when the name would not change.
Command MakeRenameCommand( Scene& scene, Entity entity, string_view wanted );

}
