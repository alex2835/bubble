#pragma once
#include "engine/editing/edit_context.hpp"
#include "engine/types/json.hpp"
#include "engine/types/string.hpp"
#include "engine/types/array.hpp"
#include "engine/types/pointer.hpp"
#include <functional>

// The editor's verbs, by name: "scene.delete", "entity.add_component". A
// menu item, a hotkey, a test and - later - a script all invoke the same
// operator with the same arguments, and the operator is the only thing that
// knows which commands that takes. Blender's bpy.ops, in miniature.
//
// An operator reads what it needs from the context (the selection, the open
// level) and from `args`, a JSON object of plain values: ids, names,
// vectors. Nothing that is a pointer into the scene, so a call can be
// written down and replayed.
namespace bubble
{
class Project;
class History;
class Selection;
class Clipboard;

// What an operator runs against. The editor builds one from its own state.
struct OperatorContext
{
    Project& mProject;
    History& mHistory;
    Selection& mSelection;
    Clipboard& mClipboard;

    EditContext Edit() const { return EditContext{ mProject, mHistory }; }
};

struct Operator
{
    string mName;  // "group.verb"
    string mLabel; // what a menu shows

    // Whether it can run now. Absent means always; a menu greys the item
    // out when this says no, and Invoke refuses to run it.
    std::function<bool( const OperatorContext&, const json& args )> mPoll;

    // Does it, through commands on the context's history.
    std::function<void( OperatorContext&, const json& args )> mExec;
};

class OperatorRegistry
{
public:
    static OperatorRegistry& Instance();

    // A name registered twice is a bug, and throws.
    void Register( Operator op );
    const Operator* Find( string_view name ) const;
    vector<string_view> Names() const;

    // Every operator the engine ships. Once, at startup; safe to repeat.
    static void RegisterBuiltins();

private:
    vector<Operator> mOperators;
};

// Unknown name throws; a failed poll returns false and does nothing.
bool PollOperator( string_view name, const OperatorContext& ctx, const json& args );
bool PollOperator( string_view name, const OperatorContext& ctx );
bool InvokeOperator( string_view name, OperatorContext& ctx, const json& args );
bool InvokeOperator( string_view name, OperatorContext& ctx );

// Operators to run later, at a point in the frame where nothing is in the
// middle of drawing the document they change: opening a level from a menu
// item pulls the level out from under every other window still drawing it,
// so the menu enqueues and the editor flushes before the next frame's UI.
class OperatorQueue
{
public:
    void Enqueue( string name, json args );
    void Enqueue( string name );

    // Runs everything queued, in order, each one logged if it throws so one
    // bad call does not drop the ones behind it.
    void Flush( OperatorContext& ctx );

    bool Empty() const { return mPending.empty(); }

private:
    struct Call
    {
        string mName;
        Scope<json> mArgs; // json is only forward-declared here
    };
    vector<Call> mPending;
};

}
