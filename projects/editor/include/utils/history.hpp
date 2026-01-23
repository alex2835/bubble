#pragma once
#include "history_commands.hpp"

namespace bubble
{
// History manager
class History
{
public:
    void ExecuteCommand( std::unique_ptr<ICommand> command )
    {
        command->Execute();

        // Clear redo stack when new command is executed
        mRedoStack.clear();

        mUndoStack.push_back( std::move( command ) );

        // Limit history size
        if ( mUndoStack.size() > mMaxHistorySize )
            mUndoStack.erase( mUndoStack.begin() );
    }

    void Undo()
    {
        if ( mUndoStack.empty() )
            return;

        auto command = std::move( mUndoStack.back() );
        mUndoStack.pop_back();

        command->Undo();

        mRedoStack.push_back( std::move( command ) );
    }

    void Redo()
    {
        if ( mRedoStack.empty() )
            return;

        auto command = std::move( mRedoStack.back() );
        mRedoStack.pop_back();

        command->Execute();

        mUndoStack.push_back( std::move( command ) );
    }

    bool CanUndo() const { return not mUndoStack.empty(); }
    bool CanRedo() const { return not mRedoStack.empty(); }

    void Clear()
    {
        mUndoStack.clear();
        mRedoStack.clear();
    }

    void SetMaxHistorySize( size_t size ) { mMaxHistorySize = size; }

private:
    std::vector<std::unique_ptr<ICommand>> mUndoStack;
    std::vector<std::unique_ptr<ICommand>> mRedoStack;
    size_t mMaxHistorySize = 50;
};

} // namespace bubble
