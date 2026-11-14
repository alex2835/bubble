#pragma once
#include "bubble/core/asset_path.hpp"
#include "bubble/editing/history.hpp"
#include "bubble/editing/selection.hpp"
#include "bubble/scene/component_types.hpp"

namespace bubble
{
// The nodes of a prefab's document by their ids, which outlast renames and
// saves: kept by Entity, whose numbers outlast undo too, and the id the next
// new node gets. A node made since the last save gets its id at the next.
struct PrefabNodes
{
    hmap<Entity, u32> mIds;
    u32 mNextId = 1;
};

// What is open in the editor and saved to a file: a level's or a prefab's
// scene, with a history of its own - undo in a prefab does not take back the
// level. The scene is read here and changed only through the edits in
// bubble/editing/edits.hpp, so every change is a step that can be undone.
//
// Stays where it is made: its history and its selection hold its scene.
class Document
{
public:
    explicit Document( const ComponentTypes& componentTypes, Scene scene = Scene() );
    Document( const Document& ) = delete;
    Document& operator=( const Document& ) = delete;

    const Scene& GetScene() const { return mScene; }
    const ComponentTypes& GetComponentTypes() const { return mComponentTypes; }
    History& GetHistory() { return mHistory; }
    const History& GetHistory() const { return mHistory; }
    Selection& GetSelection() { return mSelection; }
    const Selection& GetSelection() const { return mSelection; }

    // Whether the scene is not what was last saved. No flag is kept: the
    // history's position is compared with the one at the save, so undo back
    // to the save and the document is clean again.
    bool IsDirty() const { return mHistory.Position() != mSavedAt; }
    void MarkSaved() { mSavedAt = mHistory.Position(); }

    // The file it is saved to, from the project's root; nothing for one
    // never saved.
    OptRef<const AssetPath> TryPath() const;
    void SetPath( AssetPath path ) { mPathMaybe = std::move( path ); }

    // A prefab's document has its nodes' ids; a level's has none.
    OptRef<PrefabNodes> TryPrefabNodes();
    OptRef<const PrefabNodes> TryPrefabNodes() const;
    void SetPrefabNodes( PrefabNodes nodes ) { mPrefabNodesMaybe = std::move( nodes ); }

    // For what changes the scene and is no step of its history - instances
    // made again from a prefab saved: afterwards ForgetHistory, as the steps
    // name entities that went.
    Scene& SceneOutsideHistory() { return mScene; }
    // The scene as it is now stays, the steps go; dirty or not as before.
    void ForgetHistory();

private:
    const ComponentTypes& mComponentTypes;
    Scene mScene;
    History mHistory;
    Selection mSelection;
    u64 mSavedAt = 0;
    opt<AssetPath> mPathMaybe;
    opt<PrefabNodes> mPrefabNodesMaybe;
};
}
