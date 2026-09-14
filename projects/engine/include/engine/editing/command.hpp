#pragma once
#include "engine/types/string.hpp"
#include "engine/types/pointer.hpp"

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

}
