#pragma once
#include "bubble/editing/history.hpp"
#include "bubble/editing/selection.hpp"
#include "bubble/scene/component_types.hpp"

namespace bubble
{
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

private:
    const ComponentTypes& mComponentTypes;
    Scene mScene;
    History mHistory;
    Selection mSelection;
    u64 mSavedAt = 0;
};
}
