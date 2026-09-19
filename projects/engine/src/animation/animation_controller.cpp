#include "engine/pch/pch.hpp"
#include "engine/animation/animation_controller.hpp"
#include "engine/utils/error.hpp"
#include <nlohmann/json.hpp>
#include <charconv>

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


// AnimationController

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

    auto states = j.find( "states" );
    if ( states == j.end() or not states->is_object() or states->empty() )
        fail( "\"states\" must be an object with at least one state" );
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
                fail( std::format( "state '{}': blend \"param\" must name a parameter", name ) );
            auto points = blend->find( "points" );
            if ( points == blend->end() or not points->is_array() or points->empty() )
                fail( std::format( "state '{}': blend \"points\" must be [ [clip, value], ... ]", name ) );
            for ( const auto& point : *points )
            {
                if ( not point.is_array() or point.size() != 2 or not point[0].is_string() or not point[1].is_number() )
                    fail( std::format( "state '{}': blend \"points\" must be [ [clip, value], ... ]", name ) );
                state.mBlend.Add( point[0].get<string>(), point[1].get<f32>() );
            }
        }
        if ( state.mClip.empty() == state.mBlend.Empty() )
            fail( std::format( "state '{}': exactly one of \"clip\" and \"blend\"", name ) );

        state.mLoop = value.value( "loop", true );
        if ( auto speed = value.find( "speed" ); speed != value.end() )
        {
            if ( speed->is_number() )
                state.mSpeed = speed->get<f32>();
            else if ( speed->is_string() and hasParameter( speed->get<string>() ) )
                state.mSpeedParameter = speed->get<string>();
            else
                fail( std::format( "state '{}': \"speed\" is a number or a parameter name", name ) );
        }
        controller.mStates.push_back( std::move( state ) );
    }

    if ( auto entry = j.find( "entry" ); entry != j.end() )
    {
        controller.mEntry = controller.FindState( entry->get<string>() );
        if ( controller.mEntry < 0 )
            fail( std::format( "\"entry\" names no state: '{}'", entry->get<string>() ) );
    }

    if ( auto transitions = j.find( "transitions" ); transitions != j.end() )
    {
        if ( not transitions->is_array() )
            fail( "\"transitions\" must be an array" );
        for ( const auto& value : *transitions )
        {
            Transition transition;
            const string from = value.value( "from", string() );
            const string to = value.value( "to", string() );
            if ( from != "*" )
            {
                transition.mFrom = controller.FindState( from );
                if ( transition.mFrom < 0 )
                    fail( std::format( "transition \"from\" names no state: '{}'", from ) );
            }
            if ( to != "return" )
            {
                transition.mTo = controller.FindState( to );
                if ( transition.mTo < 0 )
                    fail( std::format( "transition \"to\" names no state: '{}'", to ) );
            }
            if ( transition.mFrom == Transition::cAnyState and transition.mTo == Transition::cReturn )
                fail( "a transition from \"*\" cannot go to \"return\"" );

            if ( auto when = value.find( "when" ); when != value.end() )
            {
                const auto add = [&]( const json& text )
                {
                    if ( not text.is_string() )
                        fail( "\"when\" is a condition string or an array of them" );
                    Condition condition = Condition::Parse( text.get<string>() );
                    if ( not hasParameter( condition.mParameter ) )
                        fail( std::format( "condition '{}' names no parameter", text.get<string>() ) );
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
                fail( std::format( "transition {} -> {} has no \"when\" and no \"exit_time\"", from, to ) );
            transition.mDuration = value.value( "duration", 0.2f );
            transition.mInterrupt = value.value( "interrupt", false );
            controller.mTransitions.push_back( std::move( transition ) );
        }
    }
    return controller;
}

i32 AnimationController::FindState( string_view name ) const
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

ControllerRuntime::Change ControllerRuntime::Enter( const AnimationController& controller, i32 state, f32 duration )
{
    BUBBLE_ASSERT( state >= 0 and state < (i32)controller.mStates.size(), "ControllerRuntime::Enter: no such state" );
    if ( state != mCurrent )
        mPrevious = mCurrent;
    mCurrent = state;
    return { state, duration };
}

bool ControllerRuntime::Satisfied( const AnimationController& controller,
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

std::optional<ControllerRuntime::Change> ControllerRuntime::Step( const AnimationController& controller,
                                                                  Parameters& parameters,
                                                                  const Frame& frame )
{
    if ( mCurrent < 0 )
        return Enter( controller, controller.mEntry, 0.0f );

    // Any-state transitions first, then the state's own, each in file order.
    const auto take = [&]( const Transition& transition ) -> std::optional<Change>
    {
        if ( not Satisfied( controller, transition, parameters, frame ) )
            return std::nullopt;
        for ( const Condition& condition : transition.mConditions )
        {
            auto iter = parameters.mValues.find( condition.mParameter );
            if ( iter != parameters.mValues.end() and iter->second.mType == Parameter::Type::Trigger )
                iter->second.mValue = 0.0f;
        }
        const i32 target = transition.mTo == Transition::cReturn ? mPrevious : transition.mTo;
        return Enter( controller, target, transition.mDuration );
    };

    for ( const Transition& transition : controller.mTransitions )
        if ( transition.mFrom == Transition::cAnyState )
            if ( auto change = take( transition ) )
                return change;
    for ( const Transition& transition : controller.mTransitions )
        if ( transition.mFrom == mCurrent )
            if ( auto change = take( transition ) )
                return change;
    return std::nullopt;
}

}
