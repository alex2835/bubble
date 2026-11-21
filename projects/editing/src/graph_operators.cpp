// The active animation graph as its editor changes it: states added, renamed
// and taken out with everything that names them, transitions drawn between
// them, where each is drawn, a parameter renamed wherever it is read. Each operator is one step of the graph's
// document, its changes taken back together when one of them fails.
#include "bubble/core/name.hpp"
#include "bubble/types/algorithm.hpp"
#include "bubble/types/format.hpp"
#include "operator_helpers.hpp"

namespace bubble
{
namespace
{
using Result = expected<void, string>;

// What a transition drawn by the mouse starts as: taken when its state's
// clip has played - one with no condition at all would be taken at once.
constexpr f64 cDrawnExitTime = 1.0;

FieldPath PathOf( string_view text )
{
    auto pathMaybe = FieldPath::TryParse( text );
    if ( not pathMaybe )
        throw logic_error( pathMaybe.error() );
    return std::move( *pathMaybe );
}

bool IsGraph( const DataDocument& document )
{
    return document.GetPath().View().ends_with( ".anim" );
}

Result PollGraph( const Workspace& workspace, const OperatorArgs& )
{
    const auto dataMaybe = workspace.TryActiveData();
    if ( not dataMaybe or not IsGraph( *dataMaybe ) )
        return unexpected( "no animation graph is active: open a .anim"s );
    return {};
}

DataDocument& Graph( Workspace& workspace )
{
    return *workspace.TryActiveData();
}

OptRef<const Json::Object> TryObjectAt( const DataDocument& document, string_view path )
{
    const auto jsonMaybe = document.TryAt( PathOf( path ) );
    return jsonMaybe ? jsonMaybe->TryAs<Json::Object>() : nullopt;
}

OptRef<const Json::Array> TryTransitions( const DataDocument& document )
{
    const auto jsonMaybe = document.GetData().TryFind( "transitions" );
    return jsonMaybe ? jsonMaybe->TryAs<Json::Array>() : nullopt;
}

// Whether a transition's `key` - from or to - is `state`.
bool Names( const Json& transition, string_view key, string_view state )
{
    const auto endMaybe = transition.TryFind( key );
    return endMaybe and endMaybe->TryAs<string>() and *endMaybe->TryAs<string>() == state;
}

bool HasState( const DataDocument& document, string_view name )
{
    const auto statesMaybe = TryObjectAt( document, "states" );
    return statesMaybe and
           ranges::any_of( *statesMaybe, [&]( const Json::Member& member ) { return member.mKey == name; } );
}

string StateNames( const DataDocument& document )
{
    string names;
    if ( const auto statesMaybe = TryObjectAt( document, "states" ) )
        for ( const Json::Member& member : *statesMaybe )
            names += ( names.empty() ? "" : ", " ) + member.mKey;
    return names.empty() ? "none" : names;
}

Result TryNoState( const DataDocument& document, string_view name )
{
    if ( not HasState( document, name ) )
        return unexpected( format( "no state {} (states: {})", name, StateNames( document ) ) );
    return {};
}

// A new state's name: snake_case, not "return" - where a transition goes
// back -, and no other state's.
Result TryNewName( const DataDocument& document, string_view name )
{
    if ( not detail::NameValid( name ) )
        return unexpected( format( "'{}' is not a state's name: a name is snake_case - a lowercase letter, then "
                                   "lowercase letters, digits and _",
                                   name ) );
    if ( name == "return" )
        return unexpected( "return is where a transition goes back to, not a state's name"s );
    if ( HasState( document, name ) )
        return unexpected( format( "there is a state {} already", name ) );
    return {};
}

// The changes of one step: all of them, or none when one fails.
Result Step( DataDocument& document, string label, const function<Result()>& change )
{
    document.BeginGroup( std::move( label ) );
    if ( auto doneMaybe = change(); not doneMaybe )
    {
        document.CancelGroup();
        return doneMaybe;
    }
    document.EndGroup();
    return {};
}

Json Point( f64 x, f64 y )
{
    return Json( Json::Array{ Json( x ), Json( y ) } );
}

// Where the editor keeps where it drew a node: a state's under its name,
// the any state's and the return's apart, as they are no states.
string PlacePath( string_view node )
{
    if ( node == "*" )
        return "editor.any";
    if ( node == "return" )
        return "editor.return";
    return format( "editor.states.{}", node );
}

// An object with one member's name changed where it stands: the order of
// states is the file's, and the first is the entry when none is named.
Json Renamed( const Json::Object& object, string_view from, string_view to )
{
    Json::Object renamed = object;
    for ( Json::Member& member : renamed )
        if ( member.mKey == from )
            member.mKey = string( to );
    return Json( std::move( renamed ) );
}

Result RunAddState( Workspace& workspace, const OperatorArgs& args )
{
    DataDocument& document = Graph( workspace );
    const auto nameMaybe = args.TryGet<Text>( "name"_n );
    const string base = nameMaybe ? nameMaybe->String() : "state";
    string name = base;
    // Left to the operator, the name is the first free one; given, it is
    // that or an error.
    for ( int suffix = 2; not nameMaybe and HasState( document, name ); ++suffix )
        name = format( "{}_{}", base, suffix );
    if ( auto freeMaybe = TryNewName( document, name ); not freeMaybe )
        return unexpected( format( "name: {}", freeMaybe.error() ) );
    const f64 x = args.TryGet<f64>( "x"_n ) ? *args.TryGet<f64>( "x"_n ) : 0.0;
    const f64 y = args.TryGet<f64>( "y"_n ) ? *args.TryGet<f64>( "y"_n ) : 0.0;
    return Step( document, format( "Add State {}", name ), [&]() -> Result {
        if ( auto setMaybe = document.TrySet( PathOf( format( "states.{}", name ) ), Json( Json::Object{} ), "" );
             not setMaybe )
            return setMaybe;
        return document.TrySet( PathOf( PlacePath( name ) ), Point( x, y ), "" );
    } );
}

Result RunRemoveState( Workspace& workspace, const OperatorArgs& args )
{
    DataDocument& document = Graph( workspace );
    const string state = args.TryGet<Text>( "state"_n )->String();
    if ( auto thereMaybe = TryNoState( document, state ); not thereMaybe )
        return unexpected( format( "state: {}", thereMaybe.error() ) );
    if ( TryObjectAt( document, "states" )->size() == 1 )
        return unexpected( format( "{} is the graph's only state: a graph has a state at least", state ) );
    return Step( document, format( "Remove State {}", state ), [&]() -> Result {
        // The transitions from it and to it, from the last: a position taken
        // out moves those after it. The data is looked at again after each
        // change, which may have moved it.
        for ( size_t at = TryTransitions( document ) ? TryTransitions( document )->size() : 0; at-- > 0; )
            if ( Names( ( *TryTransitions( document ) )[at], "from", state ) or
                 Names( ( *TryTransitions( document ) )[at], "to", state ) )
                if ( auto removedMaybe = document.TryRemove( PathOf( format( "transitions[{}]", at + 1 ) ), "" );
                     not removedMaybe )
                    return removedMaybe;
        if ( Names( document.GetData(), "entry", state ) )
            if ( auto removedMaybe = document.TryRemove( PathOf( "entry" ), "" ); not removedMaybe )
                return removedMaybe;
        if ( document.TryAt( PathOf( PlacePath( state ) ) ) )
            if ( auto removedMaybe = document.TryRemove( PathOf( PlacePath( state ) ), "" ); not removedMaybe )
                return removedMaybe;
        return document.TryRemove( PathOf( format( "states.{}", state ) ), "" );
    } );
}

Result RunRenameState( Workspace& workspace, const OperatorArgs& args )
{
    DataDocument& document = Graph( workspace );
    const string state = args.TryGet<Text>( "state"_n )->String();
    const string name = args.TryGet<Text>( "name"_n )->String();
    if ( auto thereMaybe = TryNoState( document, state ); not thereMaybe )
        return unexpected( format( "state: {}", thereMaybe.error() ) );
    if ( name == state )
        return {};
    if ( auto freeMaybe = TryNewName( document, name ); not freeMaybe )
        return unexpected( format( "name: {}", freeMaybe.error() ) );
    return Step( document, format( "Rename State {} to {}", state, name ), [&]() -> Result {
        if ( auto setMaybe =
                 document.TrySet( PathOf( "states" ), Renamed( *TryObjectAt( document, "states" ), state, name ), "" );
             not setMaybe )
            return setMaybe;
        const size_t count = TryTransitions( document ) ? TryTransitions( document )->size() : 0;
        for ( size_t at = 0; at < count; ++at )
            for ( const string_view key : { "from", "to" } )
                if ( Names( ( *TryTransitions( document ) )[at], key, state ) )
                    if ( auto setMaybe =
                             document.TrySet( PathOf( format( "transitions[{}].{}", at + 1, key ) ), Json( name ), "" );
                         not setMaybe )
                        return setMaybe;
        if ( Names( document.GetData(), "entry", state ) )
            if ( auto setMaybe = document.TrySet( PathOf( "entry" ), Json( name ), "" ); not setMaybe )
                return setMaybe;
        if ( const auto placesMaybe = TryObjectAt( document, "editor.states" ) )
            return document.TrySet( PathOf( "editor.states" ), Renamed( *placesMaybe, state, name ), "" );
        return {};
    } );
}

// A condition as the graph reads it - "name", "!name", "name <op> value" -
// with its parameter renamed when it is `from`.
string ConditionRenamed( const string& condition, string_view from, string_view to )
{
    const size_t start = condition.starts_with( '!' ) ? 1 : 0;
    const size_t end = min( condition.find( ' ', start ), condition.size() );
    if ( string_view( condition ).substr( start, end - start ) != from )
        return condition;
    return condition.substr( 0, start ) + string( to ) + condition.substr( end );
}

Result RunRenameParameter( Workspace& workspace, const OperatorArgs& args )
{
    DataDocument& document = Graph( workspace );
    const string parameter = args.TryGet<Text>( "parameter"_n )->String();
    const string name = args.TryGet<Text>( "name"_n )->String();
    const auto parametersMaybe = TryObjectAt( document, "parameters" );
    const auto has = [&]( string_view key ) {
        return parametersMaybe and
               ranges::any_of( *parametersMaybe, [&]( const Json::Member& member ) { return member.mKey == key; } );
    };
    if ( not has( parameter ) )
    {
        string names;
        if ( parametersMaybe )
            for ( const Json::Member& member : *parametersMaybe )
                names += ( names.empty() ? "" : ", " ) + member.mKey;
        return unexpected(
            format( "parameter: no parameter {} (parameters: {})", parameter, names.empty() ? "none" : names ) );
    }
    if ( name == parameter )
        return {};
    if ( not detail::NameValid( name ) )
        return unexpected( format( "name: '{}' is not a parameter's name: a name is snake_case - a lowercase letter, "
                                   "then lowercase letters, digits and _",
                                   name ) );
    if ( has( name ) )
        return unexpected( format( "name: there is a parameter {} already", name ) );
    return Step( document, format( "Rename Parameter {} to {}", parameter, name ), [&]() -> Result {
        if ( auto setMaybe =
                 document.TrySet( PathOf( "parameters" ), Renamed( *parametersMaybe, parameter, name ), "" );
             not setMaybe )
            return setMaybe;
        // Each transition's conditions, one or a list.
        const size_t count = TryTransitions( document ) ? TryTransitions( document )->size() : 0;
        for ( size_t at = 0; at < count; ++at )
        {
            const auto whenMaybe = ( *TryTransitions( document ) )[at].TryFind( "when" );
            if ( not whenMaybe )
                continue;
            Json when = *whenMaybe;
            if ( const auto textMaybe = when.TryAs<string>() )
                when = Json( ConditionRenamed( *textMaybe, parameter, name ) );
            else if ( const auto listMaybe = when.TryAs<Json::Array>() )
                for ( Json& condition : *listMaybe )
                    if ( const auto textMaybe = condition.TryAs<string>() )
                        condition = Json( ConditionRenamed( *textMaybe, parameter, name ) );
            if ( when.Write() != whenMaybe->Write() )
                if ( auto setMaybe = document.TrySet( PathOf( format( "transitions[{}].when", at + 1 ) ), when, "" );
                     not setMaybe )
                    return setMaybe;
        }
        // A blend along it, a speed it sets.
        if ( const auto statesMaybe = TryObjectAt( document, "states" ) )
            for ( const Json::Member& state : Json::Object( *statesMaybe ) )
            {
                const auto blendMaybe = state.mValue.TryFind( "blend" );
                if ( const auto paramMaybe = blendMaybe ? blendMaybe->TryFind( "param" ) : nullopt;
                     paramMaybe and paramMaybe->TryAs<string>() and *paramMaybe->TryAs<string>() == parameter )
                    if ( auto setMaybe = document.TrySet( PathOf( format( "states.{}.blend.param", state.mKey ) ),
                                                          Json( name ), "" );
                         not setMaybe )
                        return setMaybe;
                if ( const auto speedMaybe = state.mValue.TryFind( "speed" );
                     speedMaybe and speedMaybe->TryAs<string>() and *speedMaybe->TryAs<string>() == parameter )
                    if ( auto setMaybe =
                             document.TrySet( PathOf( format( "states.{}.speed", state.mKey ) ), Json( name ), "" );
                         not setMaybe )
                        return setMaybe;
            }
        return {};
    } );
}

Result RunAddTransition( Workspace& workspace, const OperatorArgs& args )
{
    DataDocument& document = Graph( workspace );
    const string from = args.TryGet<Text>( "from"_n )->String();
    const string to = args.TryGet<Text>( "to"_n )->String();
    if ( from != "*" )
        if ( auto thereMaybe = TryNoState( document, from ); not thereMaybe )
            return unexpected( format( "from: {}", thereMaybe.error() ) );
    if ( to != "return" )
        if ( auto thereMaybe = TryNoState( document, to ); not thereMaybe )
            return unexpected( format( "to: {}", thereMaybe.error() ) );
    if ( from == "*" and to == "return" )
        return unexpected( "from any state there is no return: return goes back to where a state was entered from"s );
    Json transition( Json::Object{ Json::Member{ .mKey = "from", .mValue = Json( from ) },
                                   Json::Member{ .mKey = "to", .mValue = Json( to ) },
                                   Json::Member{ .mKey = "exit_time", .mValue = Json( cDrawnExitTime ) } } );
    const string label = format( "Add Transition {} to {}", from, to );
    if ( const auto transitionsMaybe = TryTransitions( document ) )
        return document.TrySet( PathOf( format( "transitions[{}]", transitionsMaybe->size() + 1 ) ),
                                std::move( transition ), label );
    return document.TrySet( PathOf( "transitions" ), Json( Json::Array{ std::move( transition ) } ), label );
}

Result RunRemoveTransition( Workspace& workspace, const OperatorArgs& args )
{
    DataDocument& document = Graph( workspace );
    const f64 index = *args.TryGet<f64>( "index"_n );
    const auto transitionsMaybe = TryTransitions( document );
    const size_t count = transitionsMaybe ? transitionsMaybe->size() : 0;
    if ( index < 1 or index > static_cast<f64>( count ) or index != static_cast<f64>( static_cast<size_t>( index ) ) )
        return unexpected( format( "index: {} is no transition's (transitions: 1 to {})", index, count ) );
    return document.TryRemove( PathOf( format( "transitions[{}]", static_cast<size_t>( index ) ) ),
                               format( "Remove Transition {}", static_cast<size_t>( index ) ) );
}

Result RunPlace( Workspace& workspace, const OperatorArgs& args )
{
    DataDocument& document = Graph( workspace );
    const string node = args.TryGet<Text>( "state"_n )->String();
    if ( node != "*" and node != "return" )
        if ( auto thereMaybe = TryNoState( document, node ); not thereMaybe )
            return unexpected( format( "state: {}", thereMaybe.error() ) );
    return document.TrySet( PathOf( PlacePath( node ) ),
                            Point( *args.TryGet<f64>( "x"_n ), *args.TryGet<f64>( "y"_n ) ),
                            format( "Move {}", node ) );
}

Operator GraphOperator( string id,
                        string label,
                        vector<OperatorArg> args,
                        function<Result( Workspace&, const OperatorArgs& )> run )
{
    return Operator{ .mId = std::move( id ),
                     .mLabel = std::move( label ),
                     .mArgs = std::move( args ),
                     .mEdits = false,
                     .mTryPoll = PollGraph,
                     .mTryRun = std::move( run ) };
}
}

void AddGraphOperators( Operators& operators )
{
    operators.Add(
        GraphOperator( "graph.add_state", "Add State",
                       { Arg( "name", ValueKind::Text ), Arg( "x", ValueKind::Number ), Arg( "y", ValueKind::Number ) },
                       RunAddState ) );
    operators.Add( GraphOperator( "graph.remove_state", "Remove State", { Arg( "state", ValueKind::Text, cRequired ) },
                                  RunRemoveState ) );
    operators.Add( GraphOperator(
        "graph.rename_state", "Rename State",
        { Arg( "state", ValueKind::Text, cRequired ), Arg( "name", ValueKind::Text, cRequired ) }, RunRenameState ) );
    operators.Add(
        GraphOperator( "graph.rename_parameter", "Rename Parameter",
                       { Arg( "parameter", ValueKind::Text, cRequired ), Arg( "name", ValueKind::Text, cRequired ) },
                       RunRenameParameter ) );
    operators.Add( GraphOperator(
        "graph.add_transition", "Add Transition",
        { Arg( "from", ValueKind::Text, cRequired ), Arg( "to", ValueKind::Text, cRequired ) }, RunAddTransition ) );
    operators.Add( GraphOperator( "graph.remove_transition", "Remove Transition",
                                  { Arg( "index", ValueKind::Number, cRequired ) }, RunRemoveTransition ) );
    operators.Add(
        GraphOperator( "graph.place", "Move",
                       { Arg( "state", ValueKind::Text, cRequired ), Arg( "x", ValueKind::Number, cRequired ),
                         Arg( "y", ValueKind::Number, cRequired ) },
                       RunPlace ) );
}
}
