// The active data document as operators change it: a value set at a path,
// a member or an element taken out. Each is a step of that document's own
// history - or part of the group open in it.
#include "bubble/data/value_json.hpp"
#include "operator_helpers.hpp"

namespace bubble
{
namespace
{
using Result = expected<void, string>;

// A value as the file keeps it, plain: a number, a text, [x, y, z], a
// color's four numbers, an asset's id and path, an entity's path.
Json PlainJson( const Value& value, const AssetCatalog& assets )
{
    if ( const auto on = value.TryAs<bool>() )
        return Json( *on );
    if ( const auto number = value.TryAs<f64>() )
        return Json( *number );
    if ( const auto text = value.TryAs<Text>() )
        return Json( text->String() );
    if ( const auto name = value.TryAs<Name>() )
        return Json( name->ToString() );
    if ( const auto vector = value.TryAs<vec3>() )
        return Json( Json::Array{ Json( static_cast<f64>( vector->x ) ), Json( static_cast<f64>( vector->y ) ),
                                  Json( static_cast<f64>( vector->z ) ) } );
    if ( const auto color = value.TryAs<Color>() )
        return Json( Json::Array{ Json( static_cast<f64>( color->mR ) ), Json( static_cast<f64>( color->mG ) ),
                                  Json( static_cast<f64>( color->mB ) ), Json( static_cast<f64>( color->mA ) ) } );
    if ( const auto asset = value.TryAs<AssetId>() )
        return AssetToJson( *asset, assets );
    if ( const auto entity = value.TryAs<EntityPath>() )
        return Json( entity->mPath.String() );
    if ( const auto list = value.TryAs<Value::List>() )
    {
        Json::Array array;
        for ( const Value& element : *list )
            array.push_back( PlainJson( element, assets ) );
        return Json( std::move( array ) );
    }
    Json::Object object;
    for ( const ValueField& field : *value.TryAs<Value::Record>() )
        object.push_back( Json::Member{ .mKey = field.mName.ToString(), .mValue = PlainJson( field.mValue, assets ) } );
    return Json( std::move( object ) );
}

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
