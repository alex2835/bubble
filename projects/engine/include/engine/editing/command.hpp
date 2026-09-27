#pragma once
#include "engine/types/string.hpp"
#include "engine/types/pointer.hpp"
#include <vector>

namespace bubble
{
// One edit of the level or project, as the thing the user did rather than
// the fields it touched: "Delete node", "Transform.Position". Everything that
// changes the document goes through one of these, so undo, redo, the dirty
// flag and - later - driving the editor from a script all see the same
// stream. Selection, camera and panel state are not edits and never become
// commands.
//
// A command owns its data. It may hold Entity ids and tree nodes, but never a
// pointer into a component pool: the pool moves under it. It may hold a
// Scene& - the scene of a level outlives every command made in that level,
// and History is cleared when the level goes.
class ICommand
{
public:
    virtual ~ICommand() = default;

    // What the undo history shows for this step.
    virtual string_view Name() const = 0;

    // Apply, the first time.
    virtual void Execute() = 0;
    virtual void Undo() = 0;

    // Apply again after an Undo. For most commands that is Execute over
    // again - setting a value, reparenting a node. A command that *makes*
    // something overrides it: the first Execute hands out a new entity id,
    // and a redo must bring the same id back, because every later step in
    // the history names the entity by it.
    virtual void Redo() { Execute(); }
};

using Command = Scope<ICommand>;

// Several commands as one step: applied in order, undone in reverse. If one
// fails on the way, the ones before it are undone again and the failure goes
// on, so a half applied step never reaches the history.
class CompositeCommand : public ICommand
{
public:
    explicit CompositeCommand( string name ) : mName( std::move( name ) ) {}
    void Add( Command command ) { mCommands.push_back( std::move( command ) ); }
    bool Empty() const { return mCommands.empty(); }

    string_view Name() const override { return mName; }
    void Execute() override { Run( false ); }
    void Redo() override { Run( true ); }
    void Undo() override
    {
        for ( auto it = mCommands.rbegin(); it != mCommands.rend(); ++it )
            ( *it )->Undo();
    }

private:
    void Run( bool redo )
    {
        size_t done = 0;
        try
        {
            for ( ; done < mCommands.size(); done++ )
                redo ? mCommands[done]->Redo() : mCommands[done]->Execute();
        }
        catch ( ... )
        {
            while ( done > 0 )
                mCommands[--done]->Undo();
            throw;
        }
    }

    string mName;
    std::vector<Command> mCommands;
};

}
