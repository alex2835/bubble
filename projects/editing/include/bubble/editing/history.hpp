#pragma once
#include "bubble/editing/command.hpp"
#include "bubble/types/containers.hpp"
#include "bubble/types/number.hpp"
#include "bubble/types/pointer.hpp"

namespace bubble
{
// A document's undo and redo, a step at a time. A step is one command, or a
// group of them a person did as one thing: a gizmo dragged across a hundred
// frames, an operator over a hundred entities.
//
// It changes the scene it was made with: the document that owns both keeps
// them together.
class History
{
public:
    explicit History( Scene& scene ) : mScene( scene ) {}
    History( const History& ) = delete;
    History& operator=( const History& ) = delete;

    // Does `command` and keeps it: as a step called `label`, or in the open
    // group. A command that cannot be done changed nothing and is not kept.
    // A new step forgets what was undone: it cannot be redone any more.
    expected<void, string> TryDo( Scope<Command> command, string label );

    // With nothing to undo or redo they do nothing. Inside a group they are
    // a logic_error: the group is not a step yet.
    void Undo();
    void Redo();
    bool CanUndo() const { return not mUndo.empty(); }
    bool CanRedo() const { return not mRedo.empty(); }
    // What Undo would take back and Redo do again, for the menu; empty when
    // there is nothing.
    string_view UndoLabel() const;
    string_view RedoLabel() const;

    // The commands done until EndGroup are one step, `label`. A field set
    // again inside it folds into the set before, so a drag is one command
    // and not one a frame. A group nothing was done in leaves no step.
    // Groups do not nest: BeginGroup inside one is a logic_error.
    void BeginGroup( string label );
    void EndGroup();
    // Undoes what the open group did and drops it: an operator that failed
    // halfway leaves the document as it found it, redo and all.
    void CancelGroup();
    bool InGroup() const { return mGroupMaybe.has_value(); }

    // The state the scene is in, as a number: a step done, undone or redone
    // moves it, and it comes back with the state - undo back to where the
    // document was saved and it is what it was then.
    u64 Position() const;

    // The oldest steps are forgotten past this many; 100 to begin with.
    void SetMaxSteps( size_t maxSteps );

private:
    struct Step
    {
        u64 mId = 0;
        string mLabel;
        vector<Scope<Command>> mCommands;
    };

    void Push( Step step );
    void RequireNoGroup( string_view what ) const;

    Scene& mScene;
    vector<Step> mUndo;
    vector<Step> mRedo;
    opt<Step> mGroupMaybe;
    u64 mNextId = 1;
    size_t mMaxSteps = 100;
};
}
