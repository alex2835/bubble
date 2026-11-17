#include "bubble/animation/animation_graph.hpp"
#include "bubble/data/json.hpp"
#include "bubble/types/algorithm.hpp"
#include "bubble/types/format.hpp"

namespace bubble
{
namespace
{
using Failure = unexpected<string>;

Failure Fail( const Json& at, string_view what )
{
    return unexpected( format( "{}: {}", at.Where(), what ) );
}

// Every member of an object must be one of `fields`.
expected<void, string> TryKnown( const Json::Object& object,
                                 string_view where,
                                 std::initializer_list<string_view> fields )
{
    for ( const Json::Member& member : object )
        if ( find( fields.begin(), fields.end(), member.mKey ) == fields.end() )
        {
            string names;
            for ( const string_view field : fields )
                names += ( names.empty() ? "" : ", " ) + string( field );
            return Fail( member.mValue, format( "{}{}: not a field (fields: {})", where, member.mKey, names ) );
        }
    return {};
}

expected<f32, string> TryNumber( const Json& value, string_view what )
{
    if ( const auto numberMaybe = value.TryAs<f64>() )
        return static_cast<f32>( *numberMaybe );
    return Fail( value, format( "{} is a number, not {}", what, value.Describe() ) );
}

expected<bool, string> TryBool( const Json& value, string_view what )
{
    if ( const auto boolMaybe = value.TryAs<bool>() )
        return *boolMaybe;
    return Fail( value, format( "{} is true or false, not {}", what, value.Describe() ) );
}

expected<string, string> TryText( const Json& value, string_view what )
{
    if ( const auto textMaybe = value.TryAs<string>() )
        return *textMaybe;
    return Fail( value, format( "{} is a string, not {}", what, value.Describe() ) );
}

string ParameterNames( const AnimationGraph& graph )
{
    string names;
    for ( const AnimationParameter& parameter : graph.mParameters )
        names += ( names.empty() ? "" : ", " ) + parameter.mName;
    return names.empty() ? "none" : names;
}

string StateNames( const AnimationGraph& graph )
{
    string names;
    for ( const AnimationState& state : graph.mStates )
        names += ( names.empty() ? "" : ", " ) + state.mName;
    return names;
}

expected<u32, string> TryParameterOf( const AnimationGraph& graph, const Json& at, string_view name, string_view what )
{
    if ( const auto parameterMaybe = graph.TryParameter( name ) )
        return *parameterMaybe;
    return Fail( at, format( "{}: no parameter {} (parameters: {})", what, name, ParameterNames( graph ) ) );
}

expected<u32, string> TryStateOf( const AnimationGraph& graph, const Json& at, string_view name, string_view what )
{
    if ( const auto stateMaybe = graph.TryState( name ) )
        return *stateMaybe;
    return Fail( at, format( "{}: no state {} (states: {})", what, name, StateNames( graph ) ) );
}

// "speed > 1.5", "grounded", "!grounded", "grounded == false".
expected<AnimationCondition, string> TryCondition( const AnimationGraph& graph, const Json& at, string_view what )
{
    const auto textMaybe = TryText( at, what );
    if ( not textMaybe )
        return unexpected( textMaybe.error() );
    vector<string_view> tokens;
    for ( size_t from = 0; from < textMaybe->size(); )
    {
        const size_t space = min( textMaybe->find( ' ', from ), textMaybe->size() );
        if ( space > from )
            tokens.push_back( string_view( *textMaybe ).substr( from, space - from ) );
        from = space + 1;
    }
    const auto wrong = [&] {
        return Fail( at, format( "{}: \"{}\" is not \"name\", \"!name\" or \"name <op> value\"", what, *textMaybe ) );
    };
    AnimationCondition condition;
    if ( tokens.size() == 1 )
    {
        const bool negated = tokens[0].starts_with( '!' );
        const string_view name = negated ? tokens[0].substr( 1 ) : tokens[0];
        if ( name.empty() )
            return wrong();
        const auto parameterMaybe = TryParameterOf( graph, at, name, what );
        if ( not parameterMaybe )
            return unexpected( parameterMaybe.error() );
        condition.mParameter = *parameterMaybe;
        condition.mOp = negated ? AnimationCondition::Op::IsFalse : AnimationCondition::Op::IsTrue;
        return condition;
    }
    if ( tokens.size() != 3 )
        return wrong();
    using Op = AnimationCondition::Op;
    constexpr array<pair<string_view, Op>, 6> cOps{
        pair<string_view, Op>{ "==", Op::Equal },  pair<string_view, Op>{ "!=", Op::NotEqual },
        pair<string_view, Op>{ "<", Op::Less },    pair<string_view, Op>{ "<=", Op::LessOrEqual },
        pair<string_view, Op>{ ">", Op::Greater }, pair<string_view, Op>{ ">=", Op::GreaterOrEqual }
    };
    const auto op = find_if( cOps.begin(), cOps.end(), [&]( const auto& known ) { return known.first == tokens[1]; } );
    if ( op == cOps.end() )
        return wrong();
    const auto parameterMaybe = TryParameterOf( graph, at, tokens[0], what );
    if ( not parameterMaybe )
        return unexpected( parameterMaybe.error() );
    condition.mParameter = *parameterMaybe;
    condition.mOp = op->second;
    if ( tokens[2] == "true" or tokens[2] == "false" )
        condition.mValue = tokens[2] == "true" ? 1.0f : 0.0f;
    else
    {
        const auto [end, error] = from_chars( tokens[2].data(), tokens[2].data() + tokens[2].size(), condition.mValue );
        if ( error != errc() or end != tokens[2].data() + tokens[2].size() )
            return wrong();
    }
    return condition;
}

expected<void, string> TryParameters( AnimationGraph& graph, const Json& json )
{
    const auto objectMaybe = json.TryAs<Json::Object>();
    if ( not objectMaybe )
        return Fail( json, format( "parameters is an object of name: default, not {}", json.Describe() ) );
    for ( const Json::Member& member : *objectMaybe )
    {
        AnimationParameter parameter;
        parameter.mName = member.mKey;
        if ( const auto numberMaybe = member.mValue.TryAs<f64>() )
            parameter.mDefault = static_cast<f32>( *numberMaybe );
        else if ( const auto boolMaybe = member.mValue.TryAs<bool>() )
        {
            parameter.mKind = AnimationParameterKind::Bool;
            parameter.mDefault = *boolMaybe ? 1.0f : 0.0f;
        }
        else if ( const auto textMaybe = member.mValue.TryAs<string>(); textMaybe and *textMaybe == "trigger" )
            parameter.mKind = AnimationParameterKind::Trigger;
        else
            return Fail( member.mValue, format( "parameters.{}: a number, true or false, or \"trigger\", not {}",
                                                member.mKey, member.mValue.Describe() ) );
        graph.mParameters.push_back( std::move( parameter ) );
    }
    return {};
}

expected<void, string> TryStates( AnimationGraph& graph, const Json& json )
{
    const auto objectMaybe = json.TryAs<Json::Object>();
    if ( not objectMaybe or objectMaybe->empty() )
        return Fail( json, "states is an object with a state at least" );
    // The names first: a transition may name any of them.
    for ( const Json::Member& member : *objectMaybe )
    {
        AnimationState state;
        state.mName = member.mKey;
        graph.mStates.push_back( std::move( state ) );
    }
    for ( size_t s = 0; s < objectMaybe->size(); ++s )
    {
        const Json::Member& member = ( *objectMaybe )[s];
        AnimationState& state = graph.mStates[s];
        const string where = format( "states.{}", member.mKey );
        const auto fieldsMaybe = member.mValue.TryAs<Json::Object>();
        if ( not fieldsMaybe )
            return Fail( member.mValue, format( "{} is an object, not {}", where, member.mValue.Describe() ) );
        if ( auto knownMaybe =
                 TryKnown( *fieldsMaybe, where + ".", { "clip", "blend", "loop", "speed", "root_motion" } );
             not knownMaybe )
            return knownMaybe;
        if ( const auto rootMaybe = member.mValue.TryFind( "root_motion" ) )
        {
            const auto boolMaybe = TryBool( *rootMaybe, format( "{}.root_motion", where ) );
            if ( not boolMaybe )
                return unexpected( boolMaybe.error() );
            state.mRootMotion = *boolMaybe;
        }
        if ( const auto clipMaybe = member.mValue.TryFind( "clip" ) )
        {
            auto textMaybe = TryText( *clipMaybe, where + ".clip" );
            if ( not textMaybe )
                return unexpected( textMaybe.error() );
            state.mClip = std::move( *textMaybe );
        }
        if ( const auto blendMaybe = member.mValue.TryFind( "blend" ) )
        {
            if ( not state.mClip.empty() )
                return Fail( *blendMaybe, format( "{}: a clip or a blend, not both", where ) );
            const auto blendFieldsMaybe = blendMaybe->TryAs<Json::Object>();
            if ( not blendFieldsMaybe )
                return Fail( *blendMaybe, format( "{}.blend is an object, not {}", where, blendMaybe->Describe() ) );
            if ( auto knownMaybe = TryKnown( *blendFieldsMaybe, where + ".blend.", { "param", "points" } );
                 not knownMaybe )
                return knownMaybe;
            const auto paramMaybe = blendMaybe->TryFind( "param" );
            if ( not paramMaybe )
                return Fail( *blendMaybe, format( "{}.blend has no param", where ) );
            const auto nameMaybe = TryText( *paramMaybe, where + ".blend.param" );
            if ( not nameMaybe )
                return unexpected( nameMaybe.error() );
            const auto parameterMaybe = TryParameterOf( graph, *paramMaybe, *nameMaybe, where + ".blend.param" );
            if ( not parameterMaybe )
                return unexpected( parameterMaybe.error() );
            state.mBlendParameter = *parameterMaybe;
            const auto pointsMaybe = blendMaybe->TryFind( "points" );
            const auto listMaybe = pointsMaybe ? pointsMaybe->TryAs<Json::Array>() : nullopt;
            const string pointsWhat = format( "{}.blend.points is [ [clip, value], ... ]", where );
            if ( not listMaybe or listMaybe->empty() )
                return Fail( pointsMaybe ? *pointsMaybe : *blendMaybe, pointsWhat );
            for ( const Json& point : *listMaybe )
            {
                const auto pairMaybe = point.TryAs<Json::Array>();
                if ( not pairMaybe or pairMaybe->size() != 2 or not( *pairMaybe )[0].Is<string>() or
                     not( *pairMaybe )[1].Is<f64>() )
                    return Fail( point, pointsWhat );
                const f32 value = static_cast<f32>( *( *pairMaybe )[1].TryAs<f64>() );
                const auto at = upper_bound( state.mBlend.begin(), state.mBlend.end(), value,
                                             []( f32 v, const AnimationBlendPoint& p ) { return v < p.mValue; } );
                state.mBlend.insert(
                    at, AnimationBlendPoint{ .mClip = *( *pairMaybe )[0].TryAs<string>(), .mValue = value } );
            }
        }
        if ( const auto loopMaybe = member.mValue.TryFind( "loop" ) )
        {
            const auto boolMaybe = TryBool( *loopMaybe, format( "{}.loop", where ) );
            if ( not boolMaybe )
                return unexpected( boolMaybe.error() );
            state.mLoop = *boolMaybe;
        }
        if ( const auto speedMaybe = member.mValue.TryFind( "speed" ) )
        {
            if ( const auto numberMaybe = speedMaybe->TryAs<f64>() )
                state.mSpeed = static_cast<f32>( *numberMaybe );
            else if ( const auto nameMaybe = speedMaybe->TryAs<string>() )
            {
                const auto parameterMaybe = TryParameterOf( graph, *speedMaybe, *nameMaybe, where + ".speed" );
                if ( not parameterMaybe )
                    return unexpected( parameterMaybe.error() );
                state.mSpeedParameter = *parameterMaybe;
            }
            else
                return Fail( *speedMaybe,
                             format( "{}.speed is a number or a parameter, not {}", where, speedMaybe->Describe() ) );
        }
    }
    return {};
}

expected<void, string> TryTransitions( AnimationGraph& graph, const Json& json )
{
    const auto listMaybe = json.TryAs<Json::Array>();
    if ( not listMaybe )
        return Fail( json, format( "transitions is a list, not {}", json.Describe() ) );
    for ( size_t t = 0; t < listMaybe->size(); ++t )
    {
        const Json& value = ( *listMaybe )[t];
        const string where = format( "transitions[{}]", t );
        const auto fieldsMaybe = value.TryAs<Json::Object>();
        if ( not fieldsMaybe )
            return Fail( value, format( "{} is an object, not {}", where, value.Describe() ) );
        if ( auto knownMaybe =
                 TryKnown( *fieldsMaybe, where + ".", { "from", "to", "when", "exit_time", "duration", "interrupt" } );
             not knownMaybe )
            return knownMaybe;
        AnimationTransition transition;
        const auto fromMaybe = value.TryFind( "from" );
        const auto toMaybe = value.TryFind( "to" );
        if ( not fromMaybe or not toMaybe )
            return Fail( value, format( "{} needs a from and a to", where ) );
        const auto fromNameMaybe = TryText( *fromMaybe, where + ".from" );
        const auto toNameMaybe = TryText( *toMaybe, where + ".to" );
        if ( not fromNameMaybe )
            return unexpected( fromNameMaybe.error() );
        if ( not toNameMaybe )
            return unexpected( toNameMaybe.error() );
        if ( *fromNameMaybe != "*" )
        {
            const auto stateMaybe = TryStateOf( graph, *fromMaybe, *fromNameMaybe, where + ".from" );
            if ( not stateMaybe )
                return unexpected( stateMaybe.error() );
            transition.mFrom = *stateMaybe;
        }
        if ( *toNameMaybe != "return" )
        {
            const auto stateMaybe = TryStateOf( graph, *toMaybe, *toNameMaybe, where + ".to" );
            if ( not stateMaybe )
                return unexpected( stateMaybe.error() );
            transition.mTo = *stateMaybe;
        }
        if ( not transition.mFrom and not transition.mTo )
            return Fail( value, format( "{}: from \"*\" there is no \"return\"", where ) );
        if ( const auto whenMaybe = value.TryFind( "when" ) )
        {
            vector<const Json*> conditions;
            if ( const auto whenListMaybe = whenMaybe->TryAs<Json::Array>() )
                for ( const Json& condition : *whenListMaybe )
                    conditions.push_back( &condition );
            else
                conditions.push_back( &*whenMaybe );
            for ( const Json* condition : conditions )
            {
                auto conditionMaybe = TryCondition( graph, *condition, where + ".when" );
                if ( not conditionMaybe )
                    return unexpected( conditionMaybe.error() );
                transition.mConditions.push_back( *conditionMaybe );
            }
        }
        if ( const auto exitMaybe = value.TryFind( "exit_time" ) )
        {
            const auto numberMaybe = TryNumber( *exitMaybe, where + ".exit_time" );
            if ( not numberMaybe )
                return unexpected( numberMaybe.error() );
            transition.mExitTime = *numberMaybe;
        }
        if ( transition.mConditions.empty() and not transition.mExitTime )
            return Fail( value, format( "{} has no when and no exit_time: it would be taken at once", where ) );
        if ( const auto durationMaybe = value.TryFind( "duration" ) )
        {
            const auto numberMaybe = TryNumber( *durationMaybe, where + ".duration" );
            if ( not numberMaybe )
                return unexpected( numberMaybe.error() );
            transition.mDuration = max( *numberMaybe, 0.0f );
        }
        if ( const auto interruptMaybe = value.TryFind( "interrupt" ) )
        {
            const auto boolMaybe = TryBool( *interruptMaybe, where + ".interrupt" );
            if ( not boolMaybe )
                return unexpected( boolMaybe.error() );
            transition.mInterrupt = *boolMaybe;
        }
        graph.mTransitions.push_back( std::move( transition ) );
    }
    return {};
}

expected<void, string> TryEvents( AnimationGraph& graph, const Json& json )
{
    const auto objectMaybe = json.TryAs<Json::Object>();
    if ( not objectMaybe )
        return Fail( json, format( "events is an object of clip: [ [time, name], ... ], not {}", json.Describe() ) );
    for ( const Json::Member& member : *objectMaybe )
    {
        const string what = format( "events.{} is [ [time, name], ... ] with times from 0 to 1", member.mKey );
        const auto listMaybe = member.mValue.TryAs<Json::Array>();
        if ( not listMaybe )
            return Fail( member.mValue, what );
        vector<AnimationEvent>& events = graph.mEvents[member.mKey];
        for ( const Json& marker : *listMaybe )
        {
            const auto pairMaybe = marker.TryAs<Json::Array>();
            if ( not pairMaybe or pairMaybe->size() != 2 or not( *pairMaybe )[0].Is<f64>() or
                 not( *pairMaybe )[1].Is<string>() )
                return Fail( marker, what );
            const f64 time = *( *pairMaybe )[0].TryAs<f64>();
            if ( not( time >= 0 and time <= 1 ) )
                return Fail( marker, what );
            events.push_back(
                AnimationEvent{ .mTime = static_cast<f32>( time ), .mName = *( *pairMaybe )[1].TryAs<string>() } );
        }
        stable_sort( events.begin(), events.end(),
                     []( const AnimationEvent& a, const AnimationEvent& b ) { return a.mTime < b.mTime; } );
    }
    return {};
}

// Whether a share of the clip was reached or passed in the step, around the
// loop's seam too. A state entered at its exit time or past it - 0 - is
// past it at once.
bool Passed( f32 exit, const AnimationGraphState& state )
{
    const f32 before = state.mPhaseBefore;
    const f32 now = state.mPhase;
    if ( state.mWrapped )
        return now < before ? ( exit > before or exit <= now ) : ( exit < before or exit >= now );
    return ( exit > before and exit <= now ) or ( before == now and exit <= now );
}

// The events of `clip` between two phases, in the order they were passed:
// forwards, backwards, or around the seam either way.
void Crossed( const AnimationGraph& graph, string_view clip, const AnimationGraphState& state, vector<string>& out )
{
    const auto found = graph.mEvents.find( clip );
    const f32 before = state.mPhaseBefore;
    const f32 now = state.mPhase;
    if ( found == graph.mEvents.end() or ( before == now and not state.mWrapped ) )
        return;
    const vector<AnimationEvent>& events = found->second;
    const auto forwards = [&]( f32 from, f32 to ) {
        for ( const AnimationEvent& event : events )
            if ( event.mTime > from and event.mTime <= to )
                out.push_back( event.mName );
    };
    const auto backwards = [&]( f32 from, f32 to ) {
        for ( auto event = events.rbegin(); event != events.rend(); ++event )
            if ( event->mTime < from and event->mTime >= to )
                out.push_back( event->mName );
    };
    if ( state.mWrapped and now < before )
    {
        forwards( before, 1.0f );
        forwards( -1.0f, now );
    }
    else if ( state.mWrapped )
    {
        backwards( before, 0.0f );
        backwards( 2.0f, now );
    }
    else if ( now > before )
        forwards( before, now );
    else
        backwards( before, now );
}

void Enter( AnimationGraphState& state, i32 target, f32 easing )
{
    if ( target != state.mState )
        state.mPrevious = state.mState;
    state.mState = target;
    state.mPhaseBefore = 0;
    state.mPhase = 0;
    state.mWrapped = false;
    state.mEasingLeft = easing;
    ++state.mEntered;
    state.mEnteredEasing = easing;
}

bool Holds( const AnimationGraph& graph, const AnimationTransition& transition, const AnimationGraphState& state )
{
    const i32 target = transition.mTo ? static_cast<i32>( *transition.mTo ) : state.mPrevious;
    if ( target < 0 or target == state.mState )
        return false;
    if ( transition.mFrom and static_cast<i32>( *transition.mFrom ) != state.mState )
        return false;
    if ( state.mEasingLeft > 0 and not transition.mInterrupt )
        return false;
    if ( transition.mExitTime and not Passed( *transition.mExitTime, state ) )
        return false;
    (void)graph;
    return all_of( transition.mConditions.begin(), transition.mConditions.end(),
                   [&]( const AnimationCondition& condition ) { return condition.Holds( state.mParameters ); } );
}
}

bool AnimationCondition::Holds( const vector<f32>& parameters ) const
{
    const f32 value = parameters[mParameter];
    switch ( mOp )
    {
        case Op::IsTrue: return value != 0;
        case Op::IsFalse: return value == 0;
        case Op::Equal: return value == mValue;
        case Op::NotEqual: return value != mValue;
        case Op::Less: return value < mValue;
        case Op::LessOrEqual: return value <= mValue;
        case Op::Greater: return value > mValue;
        case Op::GreaterOrEqual: return value >= mValue;
    }
    return false;
}

opt<u32> AnimationGraph::TryParameter( string_view name ) const
{
    for ( u32 p = 0; p < mParameters.size(); ++p )
        if ( mParameters[p].mName == name )
            return p;
    return nullopt;
}

opt<u32> AnimationGraph::TryState( string_view name ) const
{
    for ( u32 s = 0; s < mStates.size(); ++s )
        if ( mStates[s].mName == name )
            return s;
    return nullopt;
}

vector<string> AnimationGraph::Clips() const
{
    vector<string> clips;
    const auto add = [&]( const string& clip ) {
        if ( not clip.empty() and find( clips.begin(), clips.end(), clip ) == clips.end() )
            clips.push_back( clip );
    };
    for ( const AnimationState& state : mStates )
    {
        add( state.mClip );
        for ( const AnimationBlendPoint& point : state.mBlend )
            add( point.mClip );
    }
    return clips;
}

expected<AnimationGraph, string> TryParseAnimationGraph( string_view text )
{
    auto jsonMaybe = Json::TryParse( text );
    if ( not jsonMaybe )
        return unexpected( std::move( jsonMaybe.error() ) );
    const Json& json = *jsonMaybe;
    const auto objectMaybe = json.TryAs<Json::Object>();
    if ( not objectMaybe )
        return Fail( json, format( "an animation graph is a JSON object, not {}", json.Describe() ) );
    if ( auto knownMaybe =
             TryKnown( *objectMaybe, "", { "parameters", "entry", "states", "transitions", "events", "editor" } );
         not knownMaybe )
        return unexpected( knownMaybe.error() );

    AnimationGraph graph;
    if ( const auto parametersMaybe = json.TryFind( "parameters" ) )
        if ( auto readMaybe = TryParameters( graph, *parametersMaybe ); not readMaybe )
            return unexpected( readMaybe.error() );
    const auto statesMaybe = json.TryFind( "states" );
    if ( not statesMaybe )
        return Fail( json, "an animation graph has states" );
    if ( auto readMaybe = TryStates( graph, *statesMaybe ); not readMaybe )
        return unexpected( readMaybe.error() );
    if ( const auto entryMaybe = json.TryFind( "entry" ) )
    {
        const auto nameMaybe = TryText( *entryMaybe, "entry" );
        if ( not nameMaybe )
            return unexpected( nameMaybe.error() );
        const auto stateMaybe = TryStateOf( graph, *entryMaybe, *nameMaybe, "entry" );
        if ( not stateMaybe )
            return unexpected( stateMaybe.error() );
        graph.mEntry = *stateMaybe;
    }
    if ( const auto transitionsMaybe = json.TryFind( "transitions" ) )
        if ( auto readMaybe = TryTransitions( graph, *transitionsMaybe ); not readMaybe )
            return unexpected( readMaybe.error() );
    if ( const auto eventsMaybe = json.TryFind( "events" ) )
        if ( auto readMaybe = TryEvents( graph, *eventsMaybe ); not readMaybe )
            return unexpected( readMaybe.error() );
    return graph;
}

vector<AnimationWeight> BlendWeights( const vector<AnimationBlendPoint>& points, f32 value )
{
    if ( points.empty() )
        return {};
    const u32 last = static_cast<u32>( points.size() - 1 );
    // NaN - a script's 0/0 - takes the first point rather than the search.
    if ( points.size() == 1 or isnan( value ) or value <= points.front().mValue )
        return { AnimationWeight{ 0, 1.0f } };
    if ( value >= points.back().mValue )
        return { AnimationWeight{ last, 1.0f } };
    const auto upper = lower_bound( points.begin(), points.end(), value,
                                    []( const AnimationBlendPoint& p, f32 v ) { return p.mValue < v; } );
    const u32 high = static_cast<u32>( upper - points.begin() );
    const u32 low = high - 1;
    const f32 span = points[high].mValue - points[low].mValue;
    // Two points at one value: the later one alone.
    if ( span <= 0 )
        return { AnimationWeight{ high, 1.0f } };
    const f32 t = ( value - points[low].mValue ) / span;
    return { AnimationWeight{ low, 1.0f - t }, AnimationWeight{ high, t } };
}

AnimationGraphState StartAnimationGraph( const AnimationGraph& graph )
{
    AnimationGraphState state;
    for ( const AnimationParameter& parameter : graph.mParameters )
        state.mParameters.push_back( parameter.mKind == AnimationParameterKind::Trigger ? 0.0f : parameter.mDefault );
    return state;
}

string_view LeadingClip( const AnimationGraph& graph, const AnimationGraphState& state )
{
    if ( state.mState < 0 )
        return {};
    const AnimationState& playing = graph.mStates[static_cast<size_t>( state.mState )];
    if ( playing.mBlend.empty() )
        return playing.mClip;
    const vector<AnimationWeight> weights = BlendWeights( playing.mBlend, state.mParameters[playing.mBlendParameter] );
    const auto heaviest = max_element( weights.begin(), weights.end(),
                                       []( const auto& a, const auto& b ) { return a.mWeight < b.mWeight; } );
    return playing.mBlend[heaviest->mPoint].mClip;
}

void StepAnimationGraph( const AnimationGraph& graph,
                         AnimationGraphState& state,
                         f32 dt,
                         f32 speed,
                         const function<opt<f32>( string_view clip )>& durationOf,
                         vector<string>& events )
{
    if ( state.mState < 0 )
        Enter( state, static_cast<i32>( graph.mEntry ), 0.0f );
    state.mEasingLeft = max( state.mEasingLeft - dt, 0.0f );

    // The state's clip goes on, at a rate its clips' lengths set: a blend
    // mostly of the run cycles about as fast as the run does.
    const AnimationState& playing = graph.mStates[static_cast<size_t>( state.mState )];
    const auto lengthOf = [&]( string_view clip ) { return clip.empty() ? 1.0f : durationOf( clip ).value_or( 1.0f ); };
    f32 duration = lengthOf( playing.mClip );
    if ( not playing.mBlend.empty() )
    {
        duration = 0;
        for ( const AnimationWeight& weight :
              BlendWeights( playing.mBlend, state.mParameters[playing.mBlendParameter] ) )
            duration += weight.mWeight * lengthOf( playing.mBlend[weight.mPoint].mClip );
    }
    const f32 rate = speed * ( playing.mSpeedParameter ? state.mParameters[*playing.mSpeedParameter] : playing.mSpeed );
    state.mPhaseBefore = state.mPhase;
    state.mWrapped = false;
    f32 phase = state.mPhase + dt * rate / max( duration, 1e-4f );
    if ( not isfinite( phase ) )
        phase = state.mPhase;
    if ( playing.mLoop )
    {
        state.mWrapped = phase >= 1.0f or phase < 0.0f;
        phase -= floor( phase );
    }
    else
        phase = clamp( phase, 0.0f, 1.0f );
    state.mPhase = phase;
    state.mPlayed = state.mState;
    state.mPlayedFrom = state.mPhaseBefore;
    state.mPlayedTo = state.mPhase;
    state.mPlayedWrapped = state.mWrapped;
    Crossed( graph, LeadingClip( graph, state ), state, events );

    // From any state first, then the state's own.
    const AnimationTransition* taken = nullptr;
    for ( const bool fromAny : { true, false } )
        for ( const AnimationTransition& transition : graph.mTransitions )
            if ( taken == nullptr and transition.mFrom.has_value() != fromAny and Holds( graph, transition, state ) )
                taken = &transition;
    if ( taken != nullptr )
    {
        for ( const AnimationCondition& condition : taken->mConditions )
            if ( graph.mParameters[condition.mParameter].mKind == AnimationParameterKind::Trigger )
                state.mParameters[condition.mParameter] = 0;
        Enter( state, taken->mTo ? static_cast<i32>( *taken->mTo ) : state.mPrevious, taken->mDuration );
    }
    for ( u32 p = 0; p < graph.mParameters.size(); ++p )
        if ( graph.mParameters[p].mKind == AnimationParameterKind::Trigger )
            state.mParameters[p] = 0;
}
}
