#include "bubble/editing/operators.hpp"
#include "bubble/data/value_json.hpp"
#include "bubble/types/algorithm.hpp"
#include "bubble/types/format.hpp"

namespace bubble
{
namespace
{
// "entity.rename": snake_case words with a dot between.
bool IsOperatorId( string_view id )
{
    const size_t dot = id.find( '.' );
    if ( dot == string_view::npos or id.find( '.', dot + 1 ) != string_view::npos )
        return false;
    return Name::TryFrom( id.substr( 0, dot ) ) and Name::TryFrom( id.substr( dot + 1 ) );
}

ValueType ArgsType( const Operator& op )
{
    vector<ValueTypeField> fields;
    for ( const OperatorArg& arg : op.mArgs )
        fields.push_back( ValueTypeField{ arg.mName, arg.mType } );
    return ValueType::RecordOf( std::move( fields ) );
}

string ArgNames( const Operator& op )
{
    string names;
    for ( const OperatorArg& arg : op.mArgs )
        names += format( "{}{}{}", names.empty() ? "" : ", ", arg.mName.ToString(), arg.mRequired ? "" : "?" );
    return names.empty() ? "none" : names;
}

expected<OperatorArgs, string> TryCheckArgs( const Operator& op, Value::Record record )
{
    const OperatorArgs args( std::move( record ) );
    for ( const OperatorArg& arg : op.mArgs )
        if ( arg.mRequired and not args.Has( arg.mName ) )
            return unexpected(
                format( "{}: {} is missing (arguments: {})", op.mId, arg.mName.ToString(), ArgNames( op ) ) );
    return args;
}

// The arguments in a JSON object; null is none.
expected<OperatorArgs, string> TryReadArgs( const Operator& op, const Json& json, const AssetCatalog& assets )
{
    if ( json.IsNull() )
        return TryCheckArgs( op, {} );
    auto valueMaybe = TryValueFromJson( json, ArgsType( op ), op.mId, assets );
    if ( not valueMaybe )
        return unexpected( std::move( valueMaybe.error() ) );
    return TryCheckArgs( op, *valueMaybe->TryAs<Value::Record>() );
}

// The same checks as for JSON - TryCheck alone would want every argument.
expected<OperatorArgs, string> TryReadArgs( const Operator& op, const Value::Record& record )
{
    for ( size_t i = 0; i < record.size(); ++i )
    {
        const ValueField& field = record[i];
        const string where = format( "{}.{}", op.mId, field.mName.ToString() );
        const auto declared = ranges::find( op.mArgs, field.mName, &OperatorArg::mName );
        if ( declared == op.mArgs.end() )
            return unexpected( format( "{}: not an argument (arguments: {})", where, ArgNames( op ) ) );
        for ( size_t j = 0; j < i; ++j )
            if ( record[j].mName == field.mName )
                return unexpected( format( "{}: given twice", where ) );
        if ( auto checkedMaybe = TryCheck( field.mValue, declared->mType, where ); not checkedMaybe )
            return unexpected( std::move( checkedMaybe.error() ) );
    }
    return TryCheckArgs( op, record );
}

unexpected<string> NoOperator( string_view id, const Operators& operators )
{
    string ids;
    for ( const Operator& op : operators.All() )
        ids += ( ids.empty() ? "" : ", " ) + op.mId;
    return unexpected( format( "{} is not an operator (operators: {})", id, ids ) );
}
}

bool OperatorArgs::Has( Name name ) const
{
    return TryFind( name ).HasValue();
}

OptRef<const Value> OperatorArgs::TryFind( Name name ) const
{
    for ( const ValueField& field : mRecord )
        if ( field.mName == name )
            return field.mValue;
    return nullopt;
}

void OperatorArgs::WrongKind( Name name, const Value& value )
{
    throw logic_error( format( "the argument {} is {}, not what it was declared", name.ToString(), value.Kind() ) );
}

void Operators::Add( Operator op )
{
    if ( not IsOperatorId( op.mId ) )
        throw logic_error( format( "Operators::Add of \"{}\": an id is snake_case words with a dot between", op.mId ) );
    if ( TryFind( op.mId ) )
        throw logic_error( format( "Operators::Add of {} twice", op.mId ) );
    if ( not op.mTryRun )
        throw logic_error( format( "Operators::Add of {}, which has nothing to run", op.mId ) );
    const auto place = ranges::upper_bound( mOperators, op.mId, {}, &Operator::mId );
    mOperators.insert( place, std::move( op ) );
}

OptRef<const Operator> Operators::TryFind( string_view id ) const
{
    const auto found = ranges::lower_bound( mOperators, id, {}, &Operator::mId );
    if ( found == mOperators.end() or found->mId != id )
        return nullopt;
    return *found;
}

expected<void, string> Operators::TryPoll( const Workspace& workspace, string_view id, const Json& args ) const
{
    const auto opMaybe = TryFind( id );
    if ( not opMaybe )
        return NoOperator( id, *this );
    const auto argsMaybe = TryReadArgs( *opMaybe, args, workspace.GetAssets() );
    if ( not argsMaybe )
        return unexpected( argsMaybe.error() );
    return TryPollChecked( workspace, *opMaybe, *argsMaybe );
}

expected<void, string> Operators::TryPoll( const Workspace& workspace, string_view id, const Value::Record& args ) const
{
    const auto opMaybe = TryFind( id );
    if ( not opMaybe )
        return NoOperator( id, *this );
    const auto argsMaybe = TryReadArgs( *opMaybe, args );
    if ( not argsMaybe )
        return unexpected( argsMaybe.error() );
    return TryPollChecked( workspace, *opMaybe, *argsMaybe );
}

expected<void, string> Operators::TryPollChecked( const Workspace& workspace,
                                                  const Operator& op,
                                                  const OperatorArgs& args ) const
{
    if ( op.mEdits and not workspace.TryActive() )
        return unexpected( format( "{}: no document is open", op.mId ) );
    if ( op.mTryPoll )
        if ( auto polledMaybe = op.mTryPoll( workspace, args ); not polledMaybe )
            return unexpected( format( "{}: {}", op.mId, polledMaybe.error() ) );
    return {};
}

expected<void, string> Operators::TryRun( Workspace& workspace, string_view id, const Json& args ) const
{
    const auto opMaybe = TryFind( id );
    if ( not opMaybe )
        return NoOperator( id, *this );
    const auto argsMaybe = TryReadArgs( *opMaybe, args, workspace.GetAssets() );
    if ( not argsMaybe )
        return unexpected( argsMaybe.error() );
    return TryRunChecked( workspace, *opMaybe, *argsMaybe );
}

expected<void, string> Operators::TryRun( Workspace& workspace, string_view id, const Value::Record& args ) const
{
    const auto opMaybe = TryFind( id );
    if ( not opMaybe )
        return NoOperator( id, *this );
    const auto argsMaybe = TryReadArgs( *opMaybe, args );
    if ( not argsMaybe )
        return unexpected( argsMaybe.error() );
    return TryRunChecked( workspace, *opMaybe, *argsMaybe );
}

expected<void, string> Operators::TryRunChecked( Workspace& workspace,
                                                 const Operator& op,
                                                 const OperatorArgs& args ) const
{
    const auto fail = [&]( string_view why ) { return unexpected( format( "{}: {}", op.mId, why ) ); };
    if ( auto polledMaybe = TryPollChecked( workspace, op, args ); not polledMaybe )
        return polledMaybe;
    const auto activeMaybe = workspace.TryActive();
    if ( not op.mEdits )
    {
        if ( auto ranMaybe = op.mTryRun( workspace, args ); not ranMaybe )
            return fail( ranMaybe.error() );
        return {};
    }

    // The group stays with the document it began in, whatever the run
    // makes active.
    History& history = activeMaybe->GetHistory();
    history.BeginGroup( op.mLabel );
    if ( auto ranMaybe = op.mTryRun( workspace, args ); not ranMaybe )
    {
        history.CancelGroup();
        return fail( ranMaybe.error() );
    }
    history.EndGroup();
    return {};
}
}
