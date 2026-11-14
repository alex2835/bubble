#include "bubble/editing/edits.hpp"
#include "bubble/editing/operators.hpp"
#include "bubble/scene/scene_file.hpp"
#include "bubble/types/algorithm.hpp"
#include "bubble/types/format.hpp"

namespace bubble
{
namespace
{
using Result = expected<void, string>;

Name Declare( string_view name )
{
    auto nameMaybe = Name::TryFrom( name );
    if ( not nameMaybe )
        throw logic_error( format( "an operator's argument: {}", nameMaybe.error() ) );
    return *nameMaybe;
}

OperatorArg Arg( string_view name, ValueKind kind, bool required = false )
{
    return OperatorArg{ .mName = Declare( name ), .mType = ValueType::Of( kind ), .mRequired = required };
}

OperatorArg Entities( bool required = false )
{
    return OperatorArg{ .mName = Declare( "entities" ),
                        .mType = ValueType::ListOf( ValueType::Of( ValueKind::Entity ) ),
                        .mRequired = required };
}

constexpr bool cRequired = true;

// The document an operator that edits works on: the run checked it is there.
Document& Active( Workspace& workspace )
{
    return *workspace.TryActive();
}

unexpected<string> NoDocument()
{
    return unexpected( "no document is open"s );
}

expected<Entity, string> TryResolveArg( const Document& document, const EntityPath& path, string_view where )
{
    auto foundMaybe = TryResolve( document.GetScene(), path );
    if ( not foundMaybe )
        return unexpected( format( "{}: {}", where, foundMaybe.error() ) );
    return *foundMaybe;
}

// `name`, or the active entity when it is left out.
expected<Entity, string> TryTarget( const Document& document, const OperatorArgs& args, Name name, string_view where )
{
    if ( const auto pathMaybe = args.TryGet<EntityPath>( name ) )
        return TryResolveArg( document, *pathMaybe, where );
    const Entity active = document.GetSelection().Active();
    if ( active == Entity::Null )
        return unexpected( format( "nothing selected to take for {}", where ) );
    return active;
}

// Which entities `entities` - or the selection when it is left out - means:
// the top of them for what takes subtrees, all of them for what sets
// components.
enum class Take
{
    Top,
    All,
};

expected<vector<Entity>, string> TryTargets( const Document& document, const OperatorArgs& args, Take take )
{
    vector<Entity> entities;
    if ( const auto listMaybe = args.TryGet<Value::List>( "entities"_n ) )
    {
        for ( size_t i = 0; i < listMaybe->size(); ++i )
        {
            auto entityMaybe =
                TryResolveArg( document, *( *listMaybe )[i].TryAs<EntityPath>(), format( "entities[{}]", i + 1 ) );
            if ( not entityMaybe )
                return unexpected( std::move( entityMaybe.error() ) );
            entities.push_back( *entityMaybe );
        }
        if ( entities.empty() )
            return unexpected( "no entities given"s );
        return take == Take::Top ? TopOf( document.GetScene(), entities ) : entities;
    }
    entities = take == Take::Top ? document.GetSelection().TopLevel() : document.GetSelection().All();
    if ( entities.empty() )
        return unexpected( "nothing selected"s );
    return entities;
}

// A place among children counts from 1, as positions do in Luau; left out,
// it is after the last.
expected<size_t, string> TryIndex( const OperatorArgs& args )
{
    const auto numberMaybe = args.TryGet<f64>( "index"_n );
    if ( not numberMaybe )
        return cAtEnd;
    if ( *numberMaybe < 1 )
        return unexpected( format( "index: a place counts from 1, not {}", *numberMaybe ) );
    return static_cast<size_t>( *numberMaybe ) - 1;
}

Text TextOr( const OperatorArgs& args, Name name, string_view fallback )
{
    const auto textMaybe = args.TryGet<Text>( name );
    return textMaybe ? *textMaybe : *Text::TryFrom( fallback );
}

void SelectAll( Selection& selection, span<const Entity> entities )
{
    selection.Clear();
    for ( const Entity entity : entities )
        selection.Add( entity );
}

// What takes entities can run when it is given some or something is
// selected.
Result PollTargets( const Workspace& workspace, const OperatorArgs& args, Take take )
{
    const auto documentMaybe = workspace.TryActive();
    if ( not documentMaybe )
        return NoDocument();
    if ( auto targetsMaybe = TryTargets( *documentMaybe, args, take ); not targetsMaybe )
        return unexpected( std::move( targetsMaybe.error() ) );
    return {};
}

Operator Taking( Take take, Operator op )
{
    op.mTryPoll = [take]( const Workspace& workspace, const OperatorArgs& args ) {
        return PollTargets( workspace, args, take );
    };
    return op;
}

Result RunCreate( Workspace& workspace, const OperatorArgs& args )
{
    Document& document = Active( workspace );
    const Scene& scene = document.GetScene();
    Entity parent = scene.Root();
    if ( const auto pathMaybe = args.TryGet<EntityPath>( "parent"_n ) )
    {
        auto parentMaybe = TryResolveArg( document, *pathMaybe, "parent" );
        if ( not parentMaybe )
            return unexpected( std::move( parentMaybe.error() ) );
        parent = *parentMaybe;
    }
    else if ( document.GetSelection().Active() != Entity::Null )
        parent = document.GetSelection().Active();
    const auto indexMaybe = TryIndex( args );
    if ( not indexMaybe )
        return unexpected( indexMaybe.error() );
    auto createdMaybe = TryCreateEntity( document, parent, TextOr( args, "name"_n, "entity" ), *indexMaybe );
    if ( not createdMaybe )
        return unexpected( std::move( createdMaybe.error() ) );
    document.GetSelection().Select( *createdMaybe );
    return {};
}

Result RunDestroy( Workspace& workspace, const OperatorArgs& args )
{
    Document& document = Active( workspace );
    auto targetsMaybe = TryTargets( document, args, Take::Top );
    if ( not targetsMaybe )
        return unexpected( std::move( targetsMaybe.error() ) );
    for ( const Entity entity : *targetsMaybe )
        if ( auto destroyedMaybe = TryDestroyEntity( document, entity ); not destroyedMaybe )
            return destroyedMaybe;
    return {};
}

Result RunRename( Workspace& workspace, const OperatorArgs& args )
{
    Document& document = Active( workspace );
    auto targetMaybe = TryTarget( document, args, "entity"_n, "entity" );
    if ( not targetMaybe )
        return unexpected( std::move( targetMaybe.error() ) );
    if ( auto renamedMaybe = TryRenameEntity( document, *targetMaybe, *args.TryGet<Text>( "name"_n ) );
         not renamedMaybe )
        return unexpected( std::move( renamedMaybe.error() ) );
    return {};
}

// The entities go under `parent` one after another, in the tree's order, at
// `index` among the children it has without them: the child that is there
// now is the one they go in front of, wherever the moves put it.
Result RunMove( Workspace& workspace, const OperatorArgs& args )
{
    Document& document = Active( workspace );
    const Scene& scene = document.GetScene();
    auto targetsMaybe = TryTargets( document, args, Take::Top );
    if ( not targetsMaybe )
        return unexpected( std::move( targetsMaybe.error() ) );
    auto parentMaybe = TryResolveArg( document, *args.TryGet<EntityPath>( "parent"_n ), "parent" );
    if ( not parentMaybe )
        return unexpected( std::move( parentMaybe.error() ) );
    const auto indexMaybe = TryIndex( args );
    if ( not indexMaybe )
        return unexpected( indexMaybe.error() );

    vector<Entity> others;
    for ( const Entity child : ChildrenOf( scene, *parentMaybe ) )
        if ( ranges::find( *targetsMaybe, child ) == targetsMaybe->end() )
            others.push_back( child );
    const Entity before = *indexMaybe < others.size() ? others[*indexMaybe] : Entity::Null;

    for ( const Entity entity : *targetsMaybe )
    {
        size_t index = cAtEnd;
        if ( before != Entity::Null )
        {
            // Where `before` is among the children without this one.
            index = IndexInParent( scene, before );
            if ( ParentOf( scene, entity ) == *parentMaybe and IndexInParent( scene, entity ) < index )
                --index;
        }
        if ( auto movedMaybe = TryMoveEntity( document, entity, *parentMaybe, index ); not movedMaybe )
            return movedMaybe;
    }
    return {};
}

Result RunDuplicate( Workspace& workspace, const OperatorArgs& args )
{
    Document& document = Active( workspace );
    auto targetsMaybe = TryTargets( document, args, Take::Top );
    if ( not targetsMaybe )
        return unexpected( std::move( targetsMaybe.error() ) );
    auto copiesMaybe = TryDuplicateEntities( document, *targetsMaybe );
    if ( not copiesMaybe )
        return unexpected( std::move( copiesMaybe.error() ) );
    SelectAll( document.GetSelection(), *copiesMaybe );
    return {};
}

// A new entity where the first of them is, with all of them in it.
Result RunGroup( Workspace& workspace, const OperatorArgs& args )
{
    Document& document = Active( workspace );
    const Scene& scene = document.GetScene();
    auto targetsMaybe = TryTargets( document, args, Take::Top );
    if ( not targetsMaybe )
        return unexpected( std::move( targetsMaybe.error() ) );
    const Entity first = targetsMaybe->front();
    if ( first == scene.Root() )
        return unexpected( "the root is the scene: it goes in no group"s );
    auto groupMaybe = TryCreateEntity( document, ParentOf( scene, first ), TextOr( args, "name"_n, "group" ),
                                       IndexInParent( scene, first ) );
    if ( not groupMaybe )
        return unexpected( std::move( groupMaybe.error() ) );
    for ( const Entity entity : *targetsMaybe )
        if ( auto movedMaybe = TryMoveEntity( document, entity, *groupMaybe ); not movedMaybe )
            return movedMaybe;
    document.GetSelection().Select( *groupMaybe );
    return {};
}

// Its children where it was, in their order, and it is gone.
Result RunUngroup( Workspace& workspace, const OperatorArgs& args )
{
    Document& document = Active( workspace );
    const Scene& scene = document.GetScene();
    auto groupMaybe = TryTarget( document, args, "entity"_n, "entity" );
    if ( not groupMaybe )
        return unexpected( std::move( groupMaybe.error() ) );
    if ( *groupMaybe == scene.Root() )
        return unexpected( "the root is the scene: it cannot be ungrouped"s );
    const Entity parent = ParentOf( scene, *groupMaybe );
    const size_t index = IndexInParent( scene, *groupMaybe );
    const vector<Entity> children( ChildrenOf( scene, *groupMaybe ).begin(), ChildrenOf( scene, *groupMaybe ).end() );
    for ( size_t i = 0; i < children.size(); ++i )
        if ( auto movedMaybe = TryMoveEntity( document, children[i], parent, index + i ); not movedMaybe )
            return movedMaybe;
    if ( auto destroyedMaybe = TryDestroyEntity( document, *groupMaybe ); not destroyedMaybe )
        return destroyedMaybe;
    SelectAll( document.GetSelection(), children );
    return {};
}

Result RunComponents( Workspace& workspace, const OperatorArgs& args, bool add )
{
    Document& document = Active( workspace );
    auto targetsMaybe = TryTargets( document, args, Take::All );
    if ( not targetsMaybe )
        return unexpected( std::move( targetsMaybe.error() ) );
    const Name component = *args.TryGet<Name>( "component"_n );
    for ( const Entity entity : *targetsMaybe )
        if ( auto doneMaybe = add ? TryAddComponent( document, entity, component )
                                  : TryRemoveComponent( document, entity, component );
             not doneMaybe )
            return doneMaybe;
    return {};
}

Result RunSetProperty( Workspace& workspace, const OperatorArgs& args )
{
    Document& document = Active( workspace );
    auto targetsMaybe = TryTargets( document, args, Take::All );
    if ( not targetsMaybe )
        return unexpected( std::move( targetsMaybe.error() ) );
    const auto pathMaybe = FieldPath::TryParse( args.TryGet<Text>( "path"_n )->View() );
    if ( not pathMaybe )
        return unexpected( format( "path: {}", pathMaybe.error() ) );
    const Name component = *args.TryGet<Name>( "component"_n );
    const Value& value = *args.TryFind( "value"_n );
    for ( const Entity entity : *targetsMaybe )
        if ( auto setMaybe = TrySetField( document, entity, component, *pathMaybe, MetaAny( value ) ); not setMaybe )
            return setMaybe;
    return {};
}

Result RunCopy( Workspace& workspace, const OperatorArgs& args )
{
    const auto documentMaybe = workspace.TryActive();
    if ( not documentMaybe )
        return NoDocument();
    auto targetsMaybe = TryTargets( *documentMaybe, args, Take::Top );
    if ( not targetsMaybe )
        return unexpected( std::move( targetsMaybe.error() ) );
    return workspace.GetClipboard().TryCopy( *documentMaybe, *targetsMaybe );
}

Result RunCut( Workspace& workspace, const OperatorArgs& args )
{
    if ( auto copiedMaybe = RunCopy( workspace, args ); not copiedMaybe )
        return copiedMaybe;
    return RunDestroy( workspace, args );
}

// Under `parent` at `index`; left out, right after the active entity, or at
// the end of the root when nothing is selected.
Result RunPaste( Workspace& workspace, const OperatorArgs& args )
{
    Document& document = Active( workspace );
    const Scene& scene = document.GetScene();
    auto indexMaybe = TryIndex( args );
    if ( not indexMaybe )
        return unexpected( indexMaybe.error() );
    Entity parent = scene.Root();
    size_t index = *indexMaybe;
    const Entity active = document.GetSelection().Active();
    if ( const auto pathMaybe = args.TryGet<EntityPath>( "parent"_n ) )
    {
        auto parentMaybe = TryResolveArg( document, *pathMaybe, "parent" );
        if ( not parentMaybe )
            return unexpected( std::move( parentMaybe.error() ) );
        parent = *parentMaybe;
    }
    else if ( active != Entity::Null and active != scene.Root() and index == cAtEnd )
    {
        parent = ParentOf( scene, active );
        index = IndexInParent( scene, active ) + 1;
    }
    auto pastedMaybe = TryPasteEntities( document, workspace.GetClipboard(), parent, index );
    if ( not pastedMaybe )
        return unexpected( std::move( pastedMaybe.error() ) );
    SelectAll( document.GetSelection(), *pastedMaybe );
    return {};
}

Result PollPaste( const Workspace& workspace, const OperatorArgs& )
{
    if ( workspace.GetClipboard().Empty() )
        return unexpected( "the clipboard is empty"s );
    return {};
}

Result PollHistory( const Workspace& workspace, bool undo )
{
    const auto documentMaybe = workspace.TryActive();
    if ( not documentMaybe )
        return NoDocument();
    const History& history = documentMaybe->GetHistory();
    if ( undo ? not history.CanUndo() : not history.CanRedo() )
        return unexpected( undo ? "nothing to undo"s : "nothing to redo"s );
    return {};
}

expected<AssetPath, string> TryPathArg( const OperatorArgs& args )
{
    auto pathMaybe = AssetPath::TryFrom( args.TryGet<Text>( "path"_n )->View() );
    if ( not pathMaybe )
        return unexpected( format( "path: {}", pathMaybe.error() ) );
    return *pathMaybe;
}

// The file's document, made active: opened again if it is open already.
Result RunOpen( Workspace& workspace, const OperatorArgs& args )
{
    auto pathMaybe = TryPathArg( args );
    if ( not pathMaybe )
        return unexpected( std::move( pathMaybe.error() ) );
    if ( const auto openMaybe = workspace.TryFind( *pathMaybe ) )
    {
        workspace.Activate( *openMaybe );
        return {};
    }
    auto textMaybe = workspace.GetFiles().TryRead( *pathMaybe );
    if ( not textMaybe )
        return unexpected( format( "{}: {}", pathMaybe->View(), textMaybe.error() ) );
    auto sceneMaybe = TryLoadScene( *textMaybe, workspace.GetComponentTypes() );
    if ( not sceneMaybe )
        return unexpected( format( "{}: {}", pathMaybe->View(), sceneMaybe.error() ) );
    Document& document = workspace.Open( std::move( *sceneMaybe ) );
    document.SetPath( std::move( *pathMaybe ) );
    return {};
}

// To `path` when it is given - from then on the document's file - and to
// its own file when not.
Result RunSave( Workspace& workspace, const OperatorArgs& args )
{
    const auto documentMaybe = workspace.TryActive();
    if ( not documentMaybe )
        return NoDocument();
    Document& document = *documentMaybe;
    AssetPath path;
    if ( args.Has( "path"_n ) )
    {
        auto pathMaybe = TryPathArg( args );
        if ( not pathMaybe )
            return unexpected( std::move( pathMaybe.error() ) );
        if ( const auto otherMaybe = workspace.TryFind( *pathMaybe ); otherMaybe and &*otherMaybe != &document )
            return unexpected( format( "{} is open as another document", pathMaybe->View() ) );
        path = std::move( *pathMaybe );
    }
    else if ( const auto ownMaybe = document.TryPath() )
        path = *ownMaybe;
    else
        return unexpected( "the document has no file yet: give it a path"s );

    auto textMaybe = TrySaveScene( document.GetScene(), workspace.GetComponentTypes() );
    if ( not textMaybe )
        return unexpected( std::move( textMaybe.error() ) );
    if ( auto writtenMaybe = workspace.GetFiles().TryWrite( path, *textMaybe ); not writtenMaybe )
        return unexpected( format( "{}: {}", path.View(), writtenMaybe.error() ) );
    document.SetPath( std::move( path ) );
    document.MarkSaved();
    return {};
}

// Refuses to lose what is not saved, unless `discard` says so.
Result RunClose( Workspace& workspace, const OperatorArgs& args )
{
    const auto documentMaybe = workspace.TryActive();
    if ( not documentMaybe )
        return NoDocument();
    const auto discardMaybe = args.TryGet<bool>( "discard"_n );
    if ( documentMaybe->IsDirty() and not( discardMaybe and *discardMaybe ) )
        return unexpected( "it has changes not saved: save it, or close it with discard"s );
    workspace.Close( *documentMaybe );
    return {};
}

Result RunSelect( Workspace& workspace, const OperatorArgs& args, bool add )
{
    const auto documentMaybe = workspace.TryActive();
    if ( not documentMaybe )
        return NoDocument();
    Selection& selection = documentMaybe->GetSelection();
    vector<Entity> entities;
    const Value::List& list = *args.TryGet<Value::List>( "entities"_n );
    for ( size_t i = 0; i < list.size(); ++i )
    {
        auto entityMaybe =
            TryResolveArg( *documentMaybe, *list[i].TryAs<EntityPath>(), format( "entities[{}]", i + 1 ) );
        if ( not entityMaybe )
            return unexpected( std::move( entityMaybe.error() ) );
        entities.push_back( *entityMaybe );
    }
    if ( not add )
        selection.Clear();
    for ( const Entity entity : entities )
        selection.Add( entity );
    return {};
}
}

void AddBuiltinOperators( Operators& operators )
{
    operators.Add( Operator{ .mId = "entity.create",
                             .mLabel = "Create",
                             .mArgs = { Arg( "parent", ValueKind::Entity ), Arg( "name", ValueKind::Text ),
                                        Arg( "index", ValueKind::Integer ) },
                             .mEdits = true,
                             .mTryPoll = {},
                             .mTryRun = RunCreate } );
    operators.Add( Taking( Take::Top, Operator{ .mId = "entity.destroy",
                                                .mLabel = "Delete",
                                                .mArgs = { Entities() },
                                                .mEdits = true,
                                                .mTryPoll = {},
                                                .mTryRun = RunDestroy } ) );
    operators.Add( Operator{ .mId = "entity.rename",
                             .mLabel = "Rename",
                             .mArgs = { Arg( "entity", ValueKind::Entity ), Arg( "name", ValueKind::Text, cRequired ) },
                             .mEdits = true,
                             .mTryPoll = {},
                             .mTryRun = RunRename } );
    operators.Add( Taking( Take::Top, Operator{ .mId = "entity.move",
                                                .mLabel = "Move",
                                                .mArgs = { Entities(), Arg( "parent", ValueKind::Entity, cRequired ),
                                                           Arg( "index", ValueKind::Integer ) },
                                                .mEdits = true,
                                                .mTryPoll = {},
                                                .mTryRun = RunMove } ) );
    operators.Add( Taking( Take::Top, Operator{ .mId = "entity.duplicate",
                                                .mLabel = "Duplicate",
                                                .mArgs = { Entities() },
                                                .mEdits = true,
                                                .mTryPoll = {},
                                                .mTryRun = RunDuplicate } ) );
    operators.Add( Taking( Take::Top, Operator{ .mId = "entity.group",
                                                .mLabel = "Group",
                                                .mArgs = { Entities(), Arg( "name", ValueKind::Text ) },
                                                .mEdits = true,
                                                .mTryPoll = {},
                                                .mTryRun = RunGroup } ) );
    operators.Add( Operator{ .mId = "entity.ungroup",
                             .mLabel = "Ungroup",
                             .mArgs = { Arg( "entity", ValueKind::Entity ) },
                             .mEdits = true,
                             .mTryPoll = {},
                             .mTryRun = RunUngroup } );
    operators.Add( Taking( Take::All, Operator{ .mId = "component.add",
                                                .mLabel = "Add Component",
                                                .mArgs = { Entities(), Arg( "component", ValueKind::Name, cRequired ) },
                                                .mEdits = true,
                                                .mTryPoll = {},
                                                .mTryRun = []( Workspace& workspace, const OperatorArgs& args ) {
                                                    return RunComponents( workspace, args, true );
                                                } } ) );
    operators.Add( Taking( Take::All, Operator{ .mId = "component.remove",
                                                .mLabel = "Remove Component",
                                                .mArgs = { Entities(), Arg( "component", ValueKind::Name, cRequired ) },
                                                .mEdits = true,
                                                .mTryPoll = {},
                                                .mTryRun = []( Workspace& workspace, const OperatorArgs& args ) {
                                                    return RunComponents( workspace, args, false );
                                                } } ) );
    operators.Add( Taking( Take::All, Operator{ .mId = "property.set",
                                                .mLabel = "Set Property",
                                                .mArgs = { Entities(), Arg( "component", ValueKind::Name, cRequired ),
                                                           Arg( "path", ValueKind::Text, cRequired ),
                                                           Arg( "value", ValueKind::Any, cRequired ) },
                                                .mEdits = true,
                                                .mTryPoll = {},
                                                .mTryRun = RunSetProperty } ) );
    operators.Add( Taking( Take::Top, Operator{ .mId = "edit.copy",
                                                .mLabel = "Copy",
                                                .mArgs = { Entities() },
                                                .mEdits = false,
                                                .mTryPoll = {},
                                                .mTryRun = RunCopy } ) );
    operators.Add( Taking( Take::Top, Operator{ .mId = "edit.cut",
                                                .mLabel = "Cut",
                                                .mArgs = { Entities() },
                                                .mEdits = true,
                                                .mTryPoll = {},
                                                .mTryRun = RunCut } ) );
    operators.Add( Operator{ .mId = "edit.paste",
                             .mLabel = "Paste",
                             .mArgs = { Arg( "parent", ValueKind::Entity ), Arg( "index", ValueKind::Integer ) },
                             .mEdits = true,
                             .mTryPoll = PollPaste,
                             .mTryRun = RunPaste } );
    operators.Add( Operator{
        .mId = "edit.undo",
        .mLabel = "Undo",
        .mArgs = {},
        .mEdits = false,
        .mTryPoll = []( const Workspace& workspace, const OperatorArgs& ) { return PollHistory( workspace, true ); },
        .mTryRun =
            []( Workspace& workspace, const OperatorArgs& ) {
                workspace.TryActive()->GetHistory().Undo();
                return Result();
            } } );
    operators.Add( Operator{
        .mId = "edit.redo",
        .mLabel = "Redo",
        .mArgs = {},
        .mEdits = false,
        .mTryPoll = []( const Workspace& workspace, const OperatorArgs& ) { return PollHistory( workspace, false ); },
        .mTryRun =
            []( Workspace& workspace, const OperatorArgs& ) {
                workspace.TryActive()->GetHistory().Redo();
                return Result();
            } } );
    operators.Add( Operator{ .mId = "document.new",
                             .mLabel = "New",
                             .mArgs = {},
                             .mEdits = false,
                             .mTryPoll = {},
                             .mTryRun = []( Workspace& workspace, const OperatorArgs& ) {
                                 workspace.Open();
                                 return Result();
                             } } );
    operators.Add( Operator{ .mId = "document.open",
                             .mLabel = "Open",
                             .mArgs = { Arg( "path", ValueKind::Text, cRequired ) },
                             .mEdits = false,
                             .mTryPoll = {},
                             .mTryRun = RunOpen } );
    operators.Add( Operator{ .mId = "document.save",
                             .mLabel = "Save",
                             .mArgs = { Arg( "path", ValueKind::Text ) },
                             .mEdits = false,
                             .mTryPoll = {},
                             .mTryRun = RunSave } );
    operators.Add( Operator{ .mId = "document.close",
                             .mLabel = "Close",
                             .mArgs = { Arg( "discard", ValueKind::Bool ) },
                             .mEdits = false,
                             .mTryPoll = {},
                             .mTryRun = RunClose } );
    operators.Add( Operator{ .mId = "selection.set",
                             .mLabel = "Select",
                             .mArgs = { Entities( cRequired ) },
                             .mEdits = false,
                             .mTryPoll = {},
                             .mTryRun = []( Workspace& workspace, const OperatorArgs& args ) {
                                 return RunSelect( workspace, args, false );
                             } } );
    operators.Add( Operator{ .mId = "selection.add",
                             .mLabel = "Add to Selection",
                             .mArgs = { Entities( cRequired ) },
                             .mEdits = false,
                             .mTryPoll = {},
                             .mTryRun = []( Workspace& workspace, const OperatorArgs& args ) {
                                 return RunSelect( workspace, args, true );
                             } } );
    operators.Add( Operator{ .mId = "selection.clear",
                             .mLabel = "Clear Selection",
                             .mArgs = {},
                             .mEdits = false,
                             .mTryPoll = {},
                             .mTryRun = []( Workspace& workspace, const OperatorArgs& ) -> Result {
                                 const auto documentMaybe = workspace.TryActive();
                                 if ( not documentMaybe )
                                     return NoDocument();
                                 documentMaybe->GetSelection().Clear();
                                 return {};
                             } } );
}
}
