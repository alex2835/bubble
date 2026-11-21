#pragma once
#include "bubble/editing/operators.hpp"

// What the built-in operators share: their arguments declared and read, the
// entities they act on.
namespace bubble
{
// An argument's name, declared: a bad one is a logic_error.
Name Declare( string_view name );
OperatorArg Arg( string_view name, ValueKind kind, bool required = false );
// `entities`: a list of paths.
OperatorArg Entities( bool required = false );
constexpr bool cRequired = true;

// The document an operator that edits works on: the run checked it is there.
Document& Active( Workspace& workspace );
unexpected<string> NoDocument();

expected<Entity, string> TryResolveArg( const Document& document, const EntityPath& path, string_view where );
// `name`, or the active entity when it is left out.
expected<Entity, string> TryTarget( const Document& document, const OperatorArgs& args, Name name, string_view where );

// Which entities `entities` - or the selection when it is left out - means:
// the top of them for what takes subtrees, all of them for what sets
// components.
enum class Take
{
    Top,
    All,
};
expected<vector<Entity>, string> TryTargets( const Document& document, const OperatorArgs& args, Take take );

// A place among children counts from 1, as positions do in Luau; left out,
// it is after the last.
expected<size_t, string> TryIndex( const OperatorArgs& args );
Text TextOr( const OperatorArgs& args, Name name, string_view fallback );
void SelectAll( Selection& selection, span<const Entity> entities );

// What takes entities can run when it is given some or something is
// selected.
expected<void, string> PollTargets( const Workspace& workspace, const OperatorArgs& args, Take take );
Operator Taking( Take take, Operator op );

// The prefab operators: prefab.instantiate, create, unpack, revert, apply.
void AddPrefabOperators( Operators& operators );
// The project's files: asset.create, rename, delete.
void AddAssetOperators( Operators& operators );
// The active data document: data.set, data.remove.
void AddDataOperators( Operators& operators );
// The active animation graph: graph.add_state, remove_state, rename_state,
// add_transition, remove_transition, place.
void AddGraphOperators( Operators& operators );
}
