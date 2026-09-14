#pragma once
#include "engine/editing/command.hpp"
#include "engine/types/array.hpp"

namespace bubble
{
// The undo stack of the open level.
class History
{
public:
    // Apply now and remember. The usual path.
    void Execute( Command command );

    // Remember without applying: for an edit an immediate-mode widget has
    // already written into the document while it was being dragged. The
    // command's Execute is what redo will call.
    void Record( Command command );

    void Undo();
    void Redo();

    bool CanUndo() const { return not mUndoStack.empty(); }
    bool CanRedo() const { return not mRedoStack.empty(); }

    // The step Undo / Redo would take next; empty when there is none.
    string_view NextUndoName() const;
    string_view NextRedoName() const;

    // Forget everything - the level these commands were made in is gone.
    void Clear();

    void SetMaxHistorySize( size_t size ) { mMaxHistorySize = size; }

private:
    void Push( Command command );

    vector<Command> mUndoStack;
    vector<Command> mRedoStack;
    size_t mMaxHistorySize = 50;
};

}
