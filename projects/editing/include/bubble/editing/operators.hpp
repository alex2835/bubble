#pragma once
#include "bubble/data/json.hpp"
#include "bubble/data/value.hpp"
#include "bubble/editing/workspace.hpp"

// Operators: the one way the editor acts. Menus, hotkeys, the tree, the
// inspector, the gizmo and the editor's Luau run operators by name, with
// arguments as JSON or as a Value record; nothing else calls the edits.
//
//     operators.TryRun( workspace, "entity.rename", *Json::TryParse( R"({ "entity": "/lamp", "name": "light" })" ) );
namespace bubble
{
// An argument an operator takes. One not required may be left out, and the
// operator says what that means: the selection, the end of the children.
struct OperatorArg
{
    Name mName;
    ValueType mType;
    bool mRequired = false;
};

// The arguments a run got, checked against the declaration.
class OperatorArgs
{
public:
    OperatorArgs() = default;
    explicit OperatorArgs( Value::Record record ) : mRecord( std::move( record ) ) {}

    bool Has( Name name ) const;
    OptRef<const Value> TryFind( Name name ) const;

    // An argument as what it was declared, or nothing when it was left out.
    // Its kind was checked against the declaration: another is a logic_error.
    template <typename T>
    OptRef<const T> TryGet( Name name ) const
    {
        const auto valueMaybe = TryFind( name );
        if ( not valueMaybe )
            return nullopt;
        const auto typedMaybe = valueMaybe->TryAs<T>();
        if ( not typedMaybe )
            WrongKind( name, *valueMaybe );
        return typedMaybe;
    }

private:
    [[noreturn]] static void WrongKind( Name name, const Value& value );

    Value::Record mRecord;
};

struct Operator
{
    // "entity.rename": snake_case, what it acts on and what it does, with a
    // dot between.
    string mId;
    // For a menu and the history: "Rename".
    string mLabel;
    vector<OperatorArg> mArgs;
    // Whether it changes the active document: then a run is one step of its
    // history, labelled as the operator is. Undo, the selection and the
    // clipboard change no document.
    bool mEdits = true;
    // Whether it can run now with these arguments, and why not: what a menu
    // greys out, with the reason in its tooltip. Empty is always.
    function<expected<void, string>( const Workspace&, const OperatorArgs& )> mTryPoll;
    function<expected<void, string>( Workspace&, const OperatorArgs& )> mTryRun;
};

class Operators
{
public:
    // A bad id, an id twice or an operator with nothing to run is a
    // logic_error.
    void Add( Operator op );
    OptRef<const Operator> TryFind( string_view id ) const;
    // By id: what a command palette lists.
    span<const Operator> All() const { return mOperators; }

    // Whether `id` can run now with `args`, and if not, why.
    expected<void, string> TryPoll( const Workspace& workspace, string_view id, const Json& args = Json() ) const;

    // Checks `args` - a JSON object, or null for none - against the
    // operator's arguments, polls it, and runs it. An operator that edits is
    // one step of the active document's history, or part of the step of the
    // operator that ran it. On an error what it did is undone, and the error
    // names the operator: "entity.rename: name: a text expected, got 3".
    expected<void, string> TryRun( Workspace& workspace, string_view id, const Json& args = Json() ) const;
    // The same with the arguments as a record - what the editor's Luau has.
    expected<void, string> TryRun( Workspace& workspace, string_view id, const Value::Record& args ) const;

private:
    expected<void, string> TryRunChecked( Workspace& workspace, const Operator& op, const OperatorArgs& args ) const;

    vector<Operator> mOperators;
};

// entity.*, component.*, property.set, edit.*, selection.*.
void AddBuiltinOperators( Operators& operators );
}
