// The project's files as operators change them: a new asset from its
// template, a file renamed or deleted with its .import. None is a step of a
// document's history: undo takes back edits, not files.
#include "bubble/import/import_file.hpp"
#include "bubble/types/algorithm.hpp"
#include "operator_helpers.hpp"

namespace bubble
{
namespace
{
using Result = expected<void, string>;

// What a new file of each kind starts as: the least its importer takes. A
// level and a prefab get their id from the next scan.
struct Template
{
    string_view mExtension;
    string_view mText;
};

constexpr array<Template, 5> cTemplates{
    Template{ ".mat", R"({
  "version": 1,
  "params": {}
}
)" },
    Template{ ".prefab", R"({
  "version": 1,
  "next_id": 1,
  "root": { "id": 0, "components": { "transform": {} } }
}
)" },
    Template{ ".scene", R"({
  "version": 1,
  "root": {}
}
)" },
    Template{ ".anim", R"({
  "parameters": {},
  "entry": "idle",
  "states": { "idle": {} },
  "transitions": []
}
)" },
    Template{ ".luau", R"(local P = props {}

type Self = typeof( P ) & { entity: entity }

function on_update( self: Self, dt: number )
end
)" },
};

expected<AssetPath, string> TryPath( const OperatorArgs& args, Name name )
{
    auto pathMaybe = AssetPath::TryFrom( args.TryGet<Text>( name )->View() );
    if ( not pathMaybe )
        return unexpected( format( "{}: {}", name.ToString(), pathMaybe.error() ) );
    return *pathMaybe;
}

bool Exists( const Workspace& workspace, const AssetPath& path )
{
    return workspace.GetFiles().TryRead( path ).has_value();
}

Result RunCreate( Workspace& workspace, const OperatorArgs& args )
{
    auto pathMaybe = TryPath( args, "path"_n );
    if ( not pathMaybe )
        return unexpected( std::move( pathMaybe.error() ) );
    const AssetPath& path = *pathMaybe;
    const auto found =
        ranges::find_if( cTemplates, [&]( const Template& made ) { return path.View().ends_with( made.mExtension ); } );
    if ( found == cTemplates.end() )
        return unexpected( format( "{}: an asset is made as .mat, .prefab, .scene, .anim or .luau", path.View() ) );
    if ( Exists( workspace, path ) )
        return unexpected( format( "{} is there already", path.View() ) );
    if ( auto writtenMaybe = workspace.GetFiles().TryWrite( path, found->mText ); not writtenMaybe )
        return unexpected( format( "{}: {}", path.View(), writtenMaybe.error() ) );
    return {};
}

Result RunRename( Workspace& workspace, const OperatorArgs& args )
{
    auto fromMaybe = TryPath( args, "path"_n );
    if ( not fromMaybe )
        return unexpected( std::move( fromMaybe.error() ) );
    auto toMaybe = TryPath( args, "to"_n );
    if ( not toMaybe )
        return unexpected( std::move( toMaybe.error() ) );
    const AssetPath& from = *fromMaybe;
    const AssetPath& to = *toMaybe;
    if ( from == to )
        return {};
    if ( Exists( workspace, to ) )
        return unexpected( format( "{} is there already", to.View() ) );
    const ProjectFiles& files = workspace.GetFiles();
    if ( auto movedMaybe = files.TryMove( from, to ); not movedMaybe )
        return unexpected( format( "{}: {}", from.View(), movedMaybe.error() ) );
    // Its id goes along in its .import; an engine file keeps its own inside.
    const AssetPath importFile = ImportFileOf( from );
    if ( Exists( workspace, importFile ) )
        if ( auto movedMaybe = files.TryMove( importFile, ImportFileOf( to ) ); not movedMaybe )
            return unexpected( format( "{}: {}", importFile.View(), movedMaybe.error() ) );
    workspace.Moved( from, to );
    return {};
}

Result RunDelete( Workspace& workspace, const OperatorArgs& args )
{
    auto pathMaybe = TryPath( args, "path"_n );
    if ( not pathMaybe )
        return unexpected( std::move( pathMaybe.error() ) );
    const AssetPath& path = *pathMaybe;
    if ( workspace.TryFind( path ) )
        return unexpected( format( "{} is open: close it first", path.View() ) );
    const ProjectFiles& files = workspace.GetFiles();
    if ( auto removedMaybe = files.TryRemove( path ); not removedMaybe )
        return unexpected( format( "{}: {}", path.View(), removedMaybe.error() ) );
    if ( const AssetPath importFile = ImportFileOf( path ); Exists( workspace, importFile ) )
        if ( auto removedMaybe = files.TryRemove( importFile ); not removedMaybe )
            return unexpected( format( "{}: {}", importFile.View(), removedMaybe.error() ) );
    workspace.Removed( path );
    return {};
}
}

void AddAssetOperators( Operators& operators )
{
    operators.Add( Operator{ .mId = "asset.create",
                             .mLabel = "New Asset",
                             .mArgs = { Arg( "path", ValueKind::Text, cRequired ) },
                             .mEdits = false,
                             .mTryPoll = {},
                             .mTryRun = RunCreate } );
    operators.Add(
        Operator{ .mId = "asset.rename",
                  .mLabel = "Rename Asset",
                  .mArgs = { Arg( "path", ValueKind::Text, cRequired ), Arg( "to", ValueKind::Text, cRequired ) },
                  .mEdits = false,
                  .mTryPoll = {},
                  .mTryRun = RunRename } );
    operators.Add( Operator{ .mId = "asset.delete",
                             .mLabel = "Delete Asset",
                             .mArgs = { Arg( "path", ValueKind::Text, cRequired ) },
                             .mEdits = false,
                             .mTryPoll = {},
                             .mTryRun = RunDelete } );
}
}
