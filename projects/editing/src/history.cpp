#include "bubble/editing/history.hpp"
#include "bubble/types/format.hpp"

namespace bubble
{
expected<void, string> History::TryDo( Scope<Command> command, string label )
{
    if ( auto doneMaybe = command->TryDo( mScene ); not doneMaybe )
        return doneMaybe;
    if ( not command->Changed() )
        return {};
    if ( mGroupMaybe )
    {
        // What was undone stays redoable until the group is a step: a
        // cancelled one puts the scene back as it was. A command folds only
        // into one of its own group, which a cancel takes back with it.
        vector<Scope<Command>>& commands = mGroupMaybe->mCommands;
        const bool folds = commands.size() > mGroupStarts.back() and commands.back()->Absorb( *command );
        if ( not folds )
            commands.push_back( std::move( command ) );
        return {};
    }
    Step step{ .mId = mNextId++, .mLabel = std::move( label ), .mCommands = {} };
    step.mCommands.push_back( std::move( command ) );
    Push( std::move( step ) );
    return {};
}

void History::Undo()
{
    RequireNoGroup( "Undo" );
    if ( mUndo.empty() )
        return;
    Step step = std::move( mUndo.back() );
    mUndo.pop_back();
    for ( auto command = step.mCommands.rbegin(); command != step.mCommands.rend(); ++command )
        ( *command )->Undo( mScene );
    mRedo.push_back( std::move( step ) );
}

void History::Redo()
{
    RequireNoGroup( "Redo" );
    if ( mRedo.empty() )
        return;
    Step step = std::move( mRedo.back() );
    mRedo.pop_back();
    for ( const Scope<Command>& command : step.mCommands )
        command->Redo( mScene );
    mUndo.push_back( std::move( step ) );
}

string_view History::UndoLabel() const
{
    return mUndo.empty() ? string_view() : mUndo.back().mLabel;
}

string_view History::RedoLabel() const
{
    return mRedo.empty() ? string_view() : mRedo.back().mLabel;
}

void History::BeginGroup( string label )
{
    if ( not mGroupMaybe )
        mGroupMaybe = Step{ .mId = mNextId++, .mLabel = std::move( label ), .mCommands = {} };
    mGroupStarts.push_back( mGroupMaybe->mCommands.size() );
}

void History::EndGroup()
{
    RequireGroup( "EndGroup" );
    mGroupStarts.pop_back();
    if ( not mGroupStarts.empty() )
        return;
    Step step = std::move( *mGroupMaybe );
    mGroupMaybe.reset();
    // A drag that came back to where it began changed nothing.
    erase_if( step.mCommands, []( const Scope<Command>& command ) { return not command->Changed(); } );
    if ( not step.mCommands.empty() )
        Push( std::move( step ) );
}

void History::CancelGroup()
{
    RequireGroup( "CancelGroup" );
    vector<Scope<Command>>& commands = mGroupMaybe->mCommands;
    while ( commands.size() > mGroupStarts.back() )
    {
        commands.back()->Undo( mScene );
        commands.pop_back();
    }
    mGroupStarts.pop_back();
    if ( mGroupStarts.empty() )
        mGroupMaybe.reset();
}

u64 History::Position() const
{
    // An open group has changed the scene already, though it is no step yet.
    if ( mGroupMaybe and not mGroupMaybe->mCommands.empty() )
        return mGroupMaybe->mId;
    return mUndo.empty() ? 0 : mUndo.back().mId;
}

void History::SetMaxSteps( size_t maxSteps )
{
    // The step just taken has to stay: an edit reads what it made from it.
    if ( maxSteps == 0 )
        throw logic_error( "SetMaxSteps( 0 ): a history keeps a step at least" );
    mMaxSteps = maxSteps;
    while ( mUndo.size() > mMaxSteps )
        mUndo.erase( mUndo.begin() );
}

void History::Clear()
{
    RequireNoGroup( "Clear" );
    mUndo.clear();
    mRedo.clear();
}

void History::Push( Step step )
{
    mRedo.clear();
    mUndo.push_back( std::move( step ) );
    if ( mUndo.size() > mMaxSteps )
        mUndo.erase( mUndo.begin() );
}

void History::RequireGroup( string_view what ) const
{
    if ( not mGroupMaybe )
        throw logic_error( format( "{} with no group open", what ) );
}

void History::RequireNoGroup( string_view what ) const
{
    if ( mGroupMaybe )
        throw logic_error( format( "{} inside the group {}, which is not a step yet", what, mGroupMaybe->mLabel ) );
}
}
