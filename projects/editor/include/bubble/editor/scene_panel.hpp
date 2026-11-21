#pragma once
#include "bubble/data/json.hpp"
#include "bubble/editing/document.hpp"
#include "bubble/editor/editor_project.hpp"

namespace bubble
{
// What the scene panel's rows carry when dragged: their entities, as
// numbers - an entity field takes one dropped on it.
constexpr const char* cEntitiesPayload = "bubble.entities";

// An operator and its arguments: what a gesture in a panel asks for.
struct OperatorCall
{
    string mId;
    Json mArgs;
};

// A click on `entity` among the tree's rows as they were shown, top to
// bottom: alone it selects the entity; with ctrl it flips it in the
// selection; with shift it selects the rows from the active one to it, it
// active - or it alone, when nothing active is shown.
OperatorCall ClickCall( const Scene& scene,
                        const Selection& selection,
                        span<const Entity> rows,
                        Entity entity,
                        bool ctrl,
                        bool shift );

// Where a drop lands on a row: above it, into it as its last child, below
// it.
enum class DropPlace : u8
{
    Before,
    Into,
    After,
};

// `dragged` dropped on `target`: moved there, in the order of the tree.
// Nothing when it cannot land there - on one of them, or under one.
opt<OperatorCall> TryDropCall( const Scene& scene, span<const Entity> dragged, Entity target, DropPlace place );

// The panel of the active document's tree: the selection by clicks - ctrl
// and shift as a file manager has them -, renaming in place (F2), moving by
// dragging, a menu of what the operators do to entities, and their keys
// while it has the focus. Instances of prefabs stand out; a double click on
// one opens its prefab. An entity made active elsewhere - the viewport, an
// operator - is shown: the rows over it open, the list scrolled to it.
class ScenePanel
{
public:
    explicit ScenePanel( EditorProject& project );

    // Its widgets, inside an ImGui window.
    void Show();

private:
    void Row( Document& document, Entity entity );
    void RowMenu( Document& document, Entity entity );
    void Rename( Document& document, Entity entity );
    void DropOn( Document& document, Entity entity, bool open );
    void Keys( Document& document );
    void MenuItem( const char* label, const char* keys, string_view op, const Json& args = Json() );
    // Asked for while the rows are walked, run once they all are, so the
    // tree holds still meanwhile.
    void Run( string_view op, const Json& args = Json() );

    EditorProject& mProject;
    vector<OperatorCall> mPending;
    // The rows as last shown, top to bottom: what shift selects across.
    vector<Entity> mRows;
    vector<Entity> mShown;
    // Being dragged.
    vector<Entity> mDragged;
    // A prefab or a model dropped on the tree, and the entity it goes under:
    // placed once the rows are walked.
    opt<pair<AssetPath, EntityPath>> mPlaceMaybe;
    // A click on a row already selected, kept until the button comes up:
    // it may be the start of a drag of them all.
    Entity mPressed = Entity::Null;
    Entity mRenaming = Entity::Null;
    // The active entity as last shown, and one to show.
    Entity mActive = Entity::Null;
    Entity mReveal = Entity::Null;
    string mNewName;
    bool mRenameStarted = false;
};
}
