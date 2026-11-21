// The active data document as operators change it: a value set at a path,
// a member or an element taken out. Each is a step of that document's own
// history - or part of the group open in it.
#include "operator_helpers.hpp"

namespace bubble
{
namespace
{
using Result = expected<void, string>;

expected<FieldPath, string> TryDataPath( const OperatorArgs& args )
{
    auto pathMaybe = FieldPath::TryParse( args.TryGet<Text>( "path"_n )->View() );
    if ( not pathMaybe )
        return unexpected( format( "path: {}", pathMaybe.error() ) );
    return *pathMaybe;
}

Result PollData( const Workspace& workspace, const OperatorArgs& )
{
    if ( not workspace.TryActiveData() )
        return unexpected( "no data document is active: open a material or an animation graph"s );
    return {};
}

Result RunSet( Workspace& workspace, const OperatorArgs& args )
{
    auto pathMaybe = TryDataPath( args );
    if ( not pathMaybe )
        return unexpected( std::move( pathMaybe.error() ) );
    return workspace.TryActiveData()->TrySet( *pathMaybe,
                                              PlainJson( *args.TryFind( "value"_n ), workspace.GetAssets() ),
                                              format( "Set {}", pathMaybe->ToString() ) );
}

Result RunRemove( Workspace& workspace, const OperatorArgs& args )
{
    auto pathMaybe = TryDataPath( args );
    if ( not pathMaybe )
        return unexpected( std::move( pathMaybe.error() ) );
    return workspace.TryActiveData()->TryRemove( *pathMaybe, format( "Remove {}", pathMaybe->ToString() ) );
}
}

void AddDataOperators( Operators& operators )
{
    operators.Add(
        Operator{ .mId = "data.set",
                  .mLabel = "Set",
                  .mArgs = { Arg( "path", ValueKind::Text, cRequired ), Arg( "value", ValueKind::Any, cRequired ) },
                  .mEdits = false,
                  .mTryPoll = PollData,
                  .mTryRun = RunSet } );
    operators.Add( Operator{ .mId = "data.remove",
                             .mLabel = "Remove",
                             .mArgs = { Arg( "path", ValueKind::Text, cRequired ) },
                             .mEdits = false,
                             .mTryPoll = PollData,
                             .mTryRun = RunRemove } );
}
}
