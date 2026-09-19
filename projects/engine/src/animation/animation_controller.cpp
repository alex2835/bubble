#include "engine/pch/pch.hpp"
#include "engine/animation/animation_controller.hpp"
#include "engine/utils/error.hpp"
#include <nlohmann/json.hpp>
#include <charconv>
#include <functional>

namespace bubble
{
// Parameters

void Parameters::Set( string_view name, f32 value )
{
    auto iter = mValues.find( name );
    if ( iter == mValues.end() )
        mValues.emplace( string( name ), Parameter{ Parameter::Type::Float, value } );
    else
        iter->second.mValue = value;
}

void Parameters::Set( string_view name, bool value )
{
    auto iter = mValues.find( name );
    if ( iter == mValues.end() )
        mValues.emplace( string( name ), Parameter{ Parameter::Type::Bool, value ? 1.0f : 0.0f } );
    else
        iter->second.mValue = value ? 1.0f : 0.0f;
}

void Parameters::Trigger( string_view name )
{
    auto iter = mValues.find( name );
    if ( iter == mValues.end() )
        mValues.emplace( string( name ), Parameter{ Parameter::Type::Trigger, 1.0f } );
    else
        iter->second.mValue = 1.0f;
}

f32 Parameters::Get( string_view name ) const
{
    auto iter = mValues.find( name );
    return iter == mValues.end() ? 0.0f : iter->second.mValue;
}

void Parameters::ResetTriggers()
{
    for ( auto& [_, parameter] : mValues )
        if ( parameter.mType == Parameter::Type::Trigger )
            parameter.mValue = 0.0f;
}


// Condition

bool Condition::Holds( const Parameters& parameters ) const
{
    const f32 value = parameters.Get( mParameter );
    switch ( mOp )
    {
    case Op::IsTrue:  return value != 0.0f;
    case Op::IsFalse: return value == 0.0f;
    case Op::Eq: return value == mValue;
    case Op::Ne: return value != mValue;
    case Op::Lt: return value < mValue;
    case Op::Le: return value <= mValue;
    case Op::Gt: return value > mValue;
    case Op::Ge: return value >= mValue;
    }
    return false;
}

Condition Condition::Parse( string_view text )
{
    vector<string_view> tokens;
    for ( const auto token : std::views::split( text, ' ' ) )
        if ( not token.empty() )
            tokens.emplace_back( token.begin(), token.end() );

    const auto fail = [&]{ throw std::runtime_error( std::format( "condition '{}': expected 'name', '!name' or 'name <op> value'", text ) ); };

    Condition condition;
    if ( tokens.size() == 1 )
    {
        if ( tokens[0].starts_with( '!' ) )
        {
            condition.mParameter = tokens[0].substr( 1 );
            condition.mOp = Op::IsFalse;
        }
        else
        {
            condition.mParameter = tokens[0];
            condition.mOp = Op::IsTrue;
        }
        if ( condition.mParameter.empty() )
            fail();
        return condition;
    }
    if ( tokens.size() != 3 )
        fail();

    condition.mParameter = tokens[0];
    static const std::pair<string_view, Op> ops[] = {
        { "==", Op::Eq }, { "!=", Op::Ne }, { "<", Op::Lt }, { "<=", Op::Le }, { ">", Op::Gt }, { ">=", Op::Ge } };
    const auto op = std::ranges::find( ops, tokens[1], &std::pair<string_view, Op>::first );
    if ( op == std::end( ops ) )
        fail();
    condition.mOp = op->second;

    if ( tokens[2] == "true" )
        condition.mValue = 1.0f;
    else if ( tokens[2] == "false" )
        condition.mValue = 0.0f;
    else
    {
        const auto [end, error] = std::from_chars( tokens[2].data(), tokens[2].data() + tokens[2].size(), condition.mValue );
        if ( error != std::errc() or end != tokens[2].data() + tokens[2].size() )
            fail();
    }
    return condition;
}

string Condition::ToString() const
{
    switch ( mOp )
    {
    case Op::IsTrue:  return mParameter;
    case Op::IsFalse: return "!" + mParameter;
    case Op::Eq: return std::format( "{} == {}", mParameter, mValue );
    case Op::Ne: return std::format( "{} != {}", mParameter, mValue );
    case Op::Lt: return std::format( "{} < {}", mParameter, mValue );
    case Op::Le: return std::format( "{} <= {}", mParameter, mValue );
    case Op::Gt: return std::format( "{} > {}", mParameter, mValue );
    case Op::Ge: return std::format( "{} >= {}", mParameter, mValue );
    }
    return mParameter;
}


// ClipEvents

void CrossedEvents( const ClipEvents& events, string_view clip,
                    f32 before, f32 now, bool wrapped, vector<string>& out )
{
    auto iter = events.find( clip );
    if ( iter == events.end() or before == now )
        return;
    const vector<ClipEvent>& markers = iter->second;

    // Forwards: the markers in (before, now], as one span or, around the
    // seam, the tail of the clip then its head.
    const auto forwards = [&]( f32 from, f32 to )
    {
        for ( const ClipEvent& e : markers )
            if ( e.mTime > from and e.mTime <= to )
                out.push_back( e.mName );
    };
    // Backwards: [now, before), latest first.
    const auto backwards = [&]( f32 from, f32 to )
    {
        for ( auto e = markers.rbegin(); e != markers.rend(); ++e )
            if ( e->mTime < from and e->mTime >= to )
                out.push_back( e->mName );
    };

    if ( wrapped )
    {
        if ( now < before )
        {
            forwards( before, 1.0f );
            forwards( -1.0f, now );
        }
        else
        {
            backwards( before, 0.0f );
            backwards( 2.0f, now );
        }
    }
    else if ( now > before )
        forwards( before, now );
    else
        backwards( before, now );
}


// AnimationController

namespace
{
// The states/entry/transitions block, for the base and for each layer.
// `where` names the block in errors; `fail` carries the file name.
void ParseStateMachine( const json& j, StateMachine& machine, const string& where,
                        const std::function<bool( const string& )>& hasParameter,
                        const std::function<void( const string& )>& fail )
{
    auto states = j.find( "states" );
    if ( states == j.end() or not states->is_object() or states->empty() )
        fail( where + "\"states\" must be an object with at least one state" );
    for ( const auto& [name, value] : states->items() )
    {
        ControllerState state;
        state.mName = name;
        if ( auto clip = value.find( "clip" ); clip != value.end() )
            state.mClip = clip->get<string>();
        if ( auto blend = value.find( "blend" ); blend != value.end() )
        {
            state.mBlendParameter = blend->value( "param", string() );
            if ( not hasParameter( state.mBlendParameter ) )
                fail( std::format( "{}state '{}': blend \"param\" must name a parameter", where, name ) );
            auto points = blend->find( "points" );
            if ( points == blend->end() or not points->is_array() or points->empty() )
                fail( std::format( "{}state '{}': blend \"points\" must be [ [clip, value], ... ]", where, name ) );
            for ( const auto& point : *points )
            {
                if ( not point.is_array() or point.size() != 2 or not point[0].is_string() or not point[1].is_number() )
                    fail( std::format( "{}state '{}': blend \"points\" must be [ [clip, value], ... ]", where, name ) );
                state.mBlend.Add( point[0].get<string>(), point[1].get<f32>() );
            }
        }
        if ( not state.mClip.empty() and not state.mBlend.Empty() )
            fail( std::format( "{}state '{}': one of \"clip\" and \"blend\", not both", where, name ) );

        state.mLoop = value.value( "loop", true );
        state.mRootMotion = value.value( "root_motion", false );
        if ( auto speed = value.find( "speed" ); speed != value.end() )
        {
            if ( speed->is_number() )
                state.mSpeed = speed->get<f32>();
            else if ( speed->is_string() and hasParameter( speed->get<string>() ) )
                state.mSpeedParameter = speed->get<string>();
            else
                fail( std::format( "{}state '{}': \"speed\" is a number or a parameter name", where, name ) );
        }
        machine.mStates.push_back( std::move( state ) );
    }

    if ( auto entry = j.find( "entry" ); entry != j.end() )
    {
        machine.mEntry = machine.FindState( entry->get<string>() );
        if ( machine.mEntry < 0 )
            fail( std::format( "{}\"entry\" names no state: '{}'", where, entry->get<string>() ) );
    }

    if ( auto transitions = j.find( "transitions" ); transitions != j.end() )
    {
        if ( not transitions->is_array() )
            fail( where + "\"transitions\" must be an array" );
        for ( const auto& value : *transitions )
        {
            Transition transition;
            const string from = value.value( "from", string() );
            const string to = value.value( "to", string() );
            if ( from != "*" )
            {
                transition.mFrom = machine.FindState( from );
                if ( transition.mFrom < 0 )
                    fail( std::format( "{}transition \"from\" names no state: '{}'", where, from ) );
            }
            if ( to != "return" )
            {
                transition.mTo = machine.FindState( to );
                if ( transition.mTo < 0 )
                    fail( std::format( "{}transition \"to\" names no state: '{}'", where, to ) );
            }
            if ( transition.mFrom == Transition::cAnyState and transition.mTo == Transition::cReturn )
                fail( where + "a transition from \"*\" cannot go to \"return\"" );

            if ( auto when = value.find( "when" ); when != value.end() )
            {
                const auto add = [&]( const json& text )
                {
                    if ( not text.is_string() )
                        fail( where + "\"when\" is a condition string or an array of them" );
                    Condition condition = Condition::Parse( text.get<string>() );
                    if ( not hasParameter( condition.mParameter ) )
                        fail( std::format( "{}condition '{}' names no parameter", where, text.get<string>() ) );
                    transition.mConditions.push_back( std::move( condition ) );
                };
                if ( when->is_array() )
                    for ( const auto& text : *when )
                        add( text );
                else
                    add( *when );
            }
            if ( auto exitTime = value.find( "exit_time" ); exitTime != value.end() )
                transition.mExitTime = exitTime->get<f32>();
            if ( transition.mConditions.empty() and not transition.mExitTime )
                fail( std::format( "{}transition {} -> {} has no \"when\" and no \"exit_time\"", where, from, to ) );
            transition.mDuration = value.value( "duration", 0.2f );
            transition.mInterrupt = value.value( "interrupt", false );
            machine.mTransitions.push_back( std::move( transition ) );
        }
    }
}
}

AnimationController AnimationController::FromJson( const json& j, const path& source )
{
    const auto fail = [&]( const string& what ) { throw std::runtime_error( std::format( "{}: {}", source.string(), what ) ); };

    AnimationController controller;
    controller.mName = source.stem().string();
    controller.mPath = source;

    if ( auto parameters = j.find( "parameters" ); parameters != j.end() )
    {
        if ( not parameters->is_object() )
            fail( "\"parameters\" must be an object of name: default" );
        for ( const auto& [name, value] : parameters->items() )
        {
            Parameter parameter;
            if ( value.is_number() )
                parameter = { Parameter::Type::Float, value.get<f32>() };
            else if ( value.is_boolean() )
                parameter = { Parameter::Type::Bool, value.get<bool>() ? 1.0f : 0.0f };
            else if ( value.is_string() and value.get<string>() == "trigger" )
                parameter = { Parameter::Type::Trigger, 0.0f };
            else
                fail( std::format( "parameter '{}': a number, true/false, or \"trigger\"", name ) );
            controller.mParameters.emplace_back( name, parameter );
        }
    }
    const auto hasParameter = [&]( const string& name )
    {
        return std::ranges::any_of( controller.mParameters, [&]( const auto& p ) { return p.first == name; } );
    };

    controller.mRootJoint = j.value( "root_joint", string() );
    ParseStateMachine( j, controller, "", hasParameter, fail );
    const auto wantsRootMotion = [&]( const StateMachine& m ) { return std::ranges::any_of( m.mStates, &ControllerState::mRootMotion ); };
    if ( controller.mRootJoint.empty() and wantsRootMotion( controller ) )
        fail( "a state with \"root_motion\" needs a \"root_joint\"" );

    if ( auto layers = j.find( "layers" ); layers != j.end() )
    {
        if ( not layers->is_array() )
            fail( "\"layers\" must be an array" );
        for ( const auto& value : *layers )
        {
            ControllerLayer layer;
            layer.mName = value.value( "name", string() );
            if ( layer.mName.empty() )
                fail( "every layer needs a \"name\"" );
            const string where = std::format( "layer '{}': ", layer.mName );
            if ( auto mask = value.find( "mask" ); mask != value.end() )
            {
                if ( mask->is_string() )
                    layer.mMask.push_back( mask->get<string>() );
                else if ( mask->is_array() )
                    for ( const auto& joint : *mask )
                        layer.mMask.push_back( joint.get<string>() );
                else
                    fail( where + "\"mask\" is a joint name or an array of them" );
            }
            if ( auto weight = value.find( "weight" ); weight != value.end() )
            {
                if ( weight->is_number() )
                    layer.mWeight = weight->get<f32>();
                else if ( weight->is_string() and hasParameter( weight->get<string>() ) )
                    layer.mWeightParameter = weight->get<string>();
                else
                    fail( where + "\"weight\" is a number or a parameter name" );
            }
            layer.mAdditive = value.value( "additive", false );
            ParseStateMachine( value, layer.mMachine, where, hasParameter, fail );
            controller.mLayers.push_back( std::move( layer ) );
        }
    }

    if ( auto events = j.find( "events" ); events != j.end() )
    {
        if ( not events->is_object() )
            fail( "\"events\" must be an object of clip: [ [time, name], ... ]" );
        for ( const auto& [clip, markers] : events->items() )
        {
            if ( not markers.is_array() )
                fail( std::format( "events '{}': [ [time, name], ... ]", clip ) );
            vector<ClipEvent>& list = controller.mEvents[clip];
            for ( const auto& marker : markers )
            {
                if ( not marker.is_array() or marker.size() != 2 or not marker[0].is_number() or not marker[1].is_string() )
                    fail( std::format( "events '{}': [ [time, name], ... ]", clip ) );
                list.push_back( { std::clamp( marker[0].get<f32>(), 0.0f, 1.0f ), marker[1].get<string>() } );
            }
            std::ranges::stable_sort( list, {}, &ClipEvent::mTime );
        }
    }
    return controller;
}

i32 StateMachine::FindState( string_view name ) const
{
    for ( size_t i = 0; i < mStates.size(); i++ )
        if ( mStates[i].mName == name )
            return static_cast<i32>( i );
    return -1;
}

Parameters AnimationController::DefaultParameters() const
{
    Parameters parameters;
    for ( const auto& [name, parameter] : mParameters )
        parameters.mValues.emplace( name, parameter );
    return parameters;
}


// ControllerRuntime

ControllerRuntime::Change ControllerRuntime::Enter( const StateMachine& machine, i32 state, f32 duration )
{
    BUBBLE_ASSERT( state >= 0 and state < (i32)machine.mStates.size(), "ControllerRuntime::Enter: no such state" );
    if ( state != mCurrent )
        mPrevious = mCurrent;
    mCurrent = state;
    return { state, duration };
}

bool ControllerRuntime::Satisfied( const StateMachine& machine,
                                   const Transition& transition,
                                   const Parameters& parameters,
                                   const Frame& frame ) const
{
    const i32 target = transition.mTo == Transition::cReturn ? mPrevious : transition.mTo;
    if ( target < 0 or target == mCurrent )
        return false;
    if ( transition.mFrom != Transition::cAnyState and transition.mFrom != mCurrent )
        return false;
    if ( frame.mInTransition and not transition.mInterrupt )
        return false;

    if ( transition.mExitTime )
    {
        const f32 exit = *transition.mExitTime;
        const f32 now = frame.mNormalizedTime;
        const f32 before = frame.mPreviousNormalizedTime;
        // Crossed this frame, either straight or around the loop's seam. A
        // state entered at or past its exit time (exit_time 0, say) fires
        // on its first frame.
        const bool crossed = frame.mWrapped
                             ? ( exit > before or exit <= now )
                             : ( ( exit > before and exit <= now ) or ( before == now and exit <= now ) );
        if ( not crossed )
            return false;
    }

    return std::ranges::all_of( transition.mConditions, [&]( const Condition& c ) { return c.Holds( parameters ); } );
}

std::optional<ControllerRuntime::Change> ControllerRuntime::Step( const StateMachine& machine,
                                                                  Parameters& parameters,
                                                                  const Frame& frame )
{
    if ( mCurrent < 0 )
        return Enter( machine, machine.mEntry, 0.0f );

    // Any-state transitions first, then the state's own, each in file order.
    const auto take = [&]( const Transition& transition ) -> std::optional<Change>
    {
        if ( not Satisfied( machine, transition, parameters, frame ) )
            return std::nullopt;
        for ( const Condition& condition : transition.mConditions )
        {
            auto iter = parameters.mValues.find( condition.mParameter );
            if ( iter != parameters.mValues.end() and iter->second.mType == Parameter::Type::Trigger )
                iter->second.mValue = 0.0f;
        }
        const i32 target = transition.mTo == Transition::cReturn ? mPrevious : transition.mTo;
        return Enter( machine, target, transition.mDuration );
    };

    for ( const Transition& transition : machine.mTransitions )
        if ( transition.mFrom == Transition::cAnyState )
            if ( auto change = take( transition ) )
                return change;
    for ( const Transition& transition : machine.mTransitions )
        if ( transition.mFrom == mCurrent )
            if ( auto change = take( transition ) )
                return change;
    return std::nullopt;
}

}
