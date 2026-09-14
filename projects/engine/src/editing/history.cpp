#include "engine/pch/pch.hpp"
#include "engine/editing/history.hpp"

namespace bubble
{
void History::Push( Command command )
{
    // A new edit forks the timeline: what was undone can no longer be redone.
    mRedoStack.clear();
    mUndoStack.push_back( std::move( command ) );
    if ( mUndoStack.size() > mMaxHistorySize )
        mUndoStack.erase( mUndoStack.begin() );
}

void History::Execute( Command command )
{
    command->Execute();
    Push( std::move( command ) );
}

void History::Record( Command command )
{
    Push( std::move( command ) );
}

void History::Undo()
{
    if ( mUndoStack.empty() )
        return;
    auto command = std::move( mUndoStack.back() );
    mUndoStack.pop_back();
    command->Undo();
    mRedoStack.push_back( std::move( command ) );
}

void History::Redo()
{
    if ( mRedoStack.empty() )
        return;
    auto command = std::move( mRedoStack.back() );
    mRedoStack.pop_back();
    command->Redo();
    mUndoStack.push_back( std::move( command ) );
}

string_view History::NextUndoName() const
{
    return mUndoStack.empty() ? string_view{} : mUndoStack.back()->Name();
}

string_view History::NextRedoName() const
{
    return mRedoStack.empty() ? string_view{} : mRedoStack.back()->Name();
}

void History::Clear()
{
    mUndoStack.clear();
    mRedoStack.clear();
}

}
