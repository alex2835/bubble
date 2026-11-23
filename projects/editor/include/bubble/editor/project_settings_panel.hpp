#pragma once
#include "bubble/editing/data_document.hpp"
#include "bubble/editor/editor_project.hpp"

namespace bubble
{
// Whether the data document is the project's project.bubble.
bool IsProjectSettings( const DataDocument& document );

// The names an input of `kind` takes in a binding - "key": "space", "a"...;
// "mouse_axis": "x", "y", "wheel" -, in the engine's order; none for "touch",
// an area, or a kind there is not.
vector<string_view> InputNames( string_view kind );

// Edit > Project Settings: project.bubble open as a data document, in a
// window of its own - the project's name, the level the game starts with,
// its actions and what each is bound to: a key, a mouse button or axis, a
// gamepad's, an area of a touch screen, each with its scale. Every edit is
// data.set or data.remove on the document: undone, saved, asked about at
// quitting as a material's are; the project reads it again once it is saved
// (EditorProject::Refresh), the next Play taking its actions.
class ProjectSettingsPanel
{
public:
    explicit ProjectSettingsPanel( EditorProject& project ) : mProject( project ) {}

    // The document's fields; it has to be the active data document for its
    // edits to go to it - the window makes it so when clicked.
    void Show( const DataDocument& document );
    // Where a field or a button was last shown, in the window's units, for
    // tests: "name", "start_level", "add_action", and by action
    // "jump.name", "jump.remove", "jump.add_binding", "jump[1].kind",
    // "jump[1].input", "jump[1].scale", "jump[1].remove".
    opt<vec2> TryWhere( string_view what ) const;

private:
    void StartLevel( const Json& data );
    void Actions( const Json& data );
    // One action's name - renamed when the field is left, in its place - and
    // its bindings.
    void Action( const Json& actions, const string& name, const Json& bindings );
    void Binding( const string& action, size_t at, const Json& binding );
    // Run once the fields are walked: they walk the document's data, which
    // an operator changes.
    void Set( const string& path, Value value );
    void Remove( const string& path );
    // The item just shown is `what`, for TryWhere.
    void Shown( string what );

    EditorProject& mProject;
    // The action whose name is being typed, and what it is so far.
    string mRenamed;
    string mRenaming;
    // What a key's list is narrowed to.
    string mFilter;
    hmap<string, vec2> mWhere;
    vector<pair<string, Value::Record>> mPending;
};
}
