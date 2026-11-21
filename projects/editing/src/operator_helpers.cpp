#include "operator_helpers.hpp"
#include "bubble/data/value_json.hpp"
#include "bubble/editing/edits.hpp"
#include "bubble/types/format.hpp"

namespace bubble
{
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

Name Declare( string_view name )
{
    auto nameMaybe = Name::TryFrom( name );
    if ( not nameMaybe )
        throw logic_error( format( "an operator's argument: {}", nameMaybe.error() ) );
    return *nameMaybe;
}

OperatorArg Arg( string_view name, ValueKind kind, bool required )
{
    return OperatorArg{ .mName = Declare( name ), .mType = ValueType::Of( kind ), .mRequired = required };
}

OperatorArg Entities( bool required )
{
    return OperatorArg{ .mName = Declare( "entities" ),
                        .mType = ValueType::ListOf( ValueType::Of( ValueKind::Entity ) ),
                        .mRequired = required };
}

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
expected<void, string> PollTargets( const Workspace& workspace, const OperatorArgs& args, Take take )
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
}
