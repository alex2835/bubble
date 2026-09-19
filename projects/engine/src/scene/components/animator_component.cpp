#include "engine/pch/pch.hpp"
#include "engine/scene/components/animator_component.hpp"
#include "engine/scene/components/model_component.hpp"
#include "engine/scene/components/component_draw_utils.hpp"
#include "engine/animation/animator.hpp"
#include "engine/project/project.hpp"
#include "engine/utils/imgui_utils.hpp"
#include <nlohmann/json.hpp>
#include <sol/sol.hpp>

namespace bubble
{
namespace
{
// { { "idle", 0 }, { "walk", 1.5 }, { "run", 4 } }: an array of clip, value
// pairs. A malformed entry is an error at the call, not a silent skip.
BlendSpace BlendSpaceFromLua( const sol::table& points )
{
    BlendSpace space;
    for ( const auto& [key, value] : points )
    {
        if ( not value.is<sol::table>() )
            throw std::runtime_error( "play_blend: each point is a { clip, value } pair" );
        const sol::table point = value.as<sol::table>();
        const sol::optional<string> clip = point[1];
        const sol::optional<f32> at = point[2];
        if ( not clip or not at )
            throw std::runtime_error( "play_blend: each point is a { clip, value } pair" );
        space.Add( *clip, *at );
    }
    if ( space.Empty() )
        throw std::runtime_error( "play_blend: no points" );
    return space;
}
}

// Out of line for the Scope<Animator>: the header only forward declares it.
AnimatorComponent::AnimatorComponent() = default;
AnimatorComponent::~AnimatorComponent() = default;
AnimatorComponent::AnimatorComponent( AnimatorComponent&& ) noexcept = default;
AnimatorComponent& AnimatorComponent::operator=( AnimatorComponent&& ) noexcept = default;

AnimatorComponent::AnimatorComponent( const AnimatorComponent& other )
    : mClip( other.mClip ),
      mTime( other.mTime ),
      mSpeed( other.mSpeed ),
      mLoop( other.mLoop ),
      mPlaying( other.mPlaying ),
      mBlend( other.mBlend ),
      mBlendValue( other.mBlendValue ),
      mController( other.mController ),
      mParameters( other.mParameters ),
      mControllerRuntime( other.mControllerRuntime )
{
}

AnimatorComponent& AnimatorComponent::operator=( const AnimatorComponent& other )
{
    if ( this != &other )
    {
        mClip = other.mClip;
        mTime = other.mTime;
        mSpeed = other.mSpeed;
        mLoop = other.mLoop;
        mPlaying = other.mPlaying;
        mBlend = other.mBlend;
        mBlendValue = other.mBlendValue;
        mController = other.mController;
        mParameters = other.mParameters;
        mControllerRuntime = other.mControllerRuntime;
        // The runtime is kept: it is bound to the model, not to the settings,
        // and is replaced by the update if the model changed.
    }
    return *this;
}

void AnimatorComponent::Play( string_view clip, f32 transition )
{
    mClip = clip;
    mBlend = {};
    mTime = 0.0f;
    mPlaying = true;
    mPendingTransition = std::max( transition, 0.0f );
}

void AnimatorComponent::PlayBlend( string_view name, BlendSpace space, f32 transition )
{
    if ( IsBlend() and mClip == name and mBlend == space )
        return;
    mClip = name;
    mBlend = std::move( space );
    mTime = 0.0f;
    mPlaying = true;
    mPendingTransition = std::max( transition, 0.0f );
}

void AnimatorComponent::Stop()
{
    mPlaying = false;
}

void AnimatorComponent::SetController( const Ref<AnimationController>& controller )
{
    mController = controller;
    mControllerRuntime = {};
    mParameters = controller ? controller->DefaultParameters() : Parameters{};
}

string_view AnimatorComponent::CurrentState() const
{
    if ( not mController or mControllerRuntime.mCurrent < 0 )
        return {};
    return mController->mStates[mControllerRuntime.mCurrent].mName;
}

bool AnimatorComponent::InTransition() const
{
    return mPendingTransition > 0.0f or ( mAnimator and mAnimator->InTransition() );
}

f32 AnimatorComponent::NormalizedTime( const Model& model ) const
{
    if ( IsBlend() )
        return mTime;
    const auto& clip = model.FindClip( mClip );
    return clip and clip->mDuration > 0.0f ? std::clamp( mTime / clip->mDuration, 0.0f, 1.0f ) : 0.0f;
}


void AnimatorComponent::Advance( const Ref<Model>& model, f32 dt )
{
    if ( not model or not model->Skinned() )
    {
        mAnimator.reset();
        return;
    }
    // Bound to the model: a new model means new joints, new meshes, new
    // buffers. Also the first frame after a load or an add_animator.
    if ( not mAnimator or mAnimator->GetModel() != model )
        mAnimator = CreateScope<Animator>( model );

    // What the controller says plays, and how: applied on entering a state,
    // and every frame for the values bound to parameters.
    const auto enter = [&]( const ControllerRuntime::Change& change )
    {
        const ControllerState& state = mController->mStates[change.mState];
        if ( state.IsBlend() )
            PlayBlend( state.mName, state.mBlend, change.mDuration );
        else
            Play( state.mClip, change.mDuration );
        mLoop = state.IsBlend() or state.mLoop;
    };
    if ( mController )
    {
        if ( mControllerRuntime.mCurrent < 0 )
            enter( mControllerRuntime.Enter( *mController, mController->mEntry, 0.0f ) );
        const ControllerState& state = mController->mStates[mControllerRuntime.mCurrent];
        if ( state.IsBlend() )
            mBlendValue = mParameters.Get( state.mBlendParameter );
        mSpeed = state.mSpeedParameter.empty() ? state.mSpeed : mParameters.Get( state.mSpeedParameter );
    }

    // Advances `time` through a cycle of `length` by this frame, looping or
    // stopping at the ends; false when it stopped.
    bool wrapped = false;
    const auto advance = [&]( f32& time, f32 length )
    {
        time += dt * mSpeed;
        if ( mLoop )
        {
            const f32 before = time;
            time = std::fmod( time, length );
            if ( time < 0.0f )
                time += length;
            wrapped = time != before;
            return true;
        }
        if ( time < length and time > 0.0f )
            return true;
        // Reached either end - the speed may be negative.
        time = std::clamp( time, 0.0f, length );
        return false;
    };

    // The blend's layers and the length of its shared cycle at the current
    // parameter, or the clip's alone.
    vector<Animator::Layer> layers;
    f32 duration = 0.0f;
    const auto resolve = [&]
    {
        layers.clear();
        duration = 0.0f;
        if ( IsBlend() )
        {
            for ( const auto& [point, weight] : mBlend.Weights( mBlendValue ) )
            {
                const auto& clip = model->FindClip( mBlend.mPoints[point].mClip );
                if ( clip )
                {
                    duration += weight * clip->mDuration;
                    layers.push_back( { clip.get(), 0.0f, weight } );
                }
            }
        }
        else if ( const auto& clip = model->FindClip( mClip ) )
        {
            duration = clip->mDuration;
            layers.push_back( { clip.get(), 0.0f, 1.0f } );
        }
    };
    // Where the playback is, 0..1, for the layers' ratios and the
    // controller's exit times.
    const auto normalized = [&]
    {
        if ( IsBlend() )
            return mTime;
        return duration > 0.0f ? std::clamp( mTime / duration, 0.0f, 1.0f ) : 0.0f;
    };

    resolve();
    const f32 previousNormalized = normalized();
    if ( duration > 0.0f and mPlaying )
    {
        if ( IsBlend() )
        {
            // The phase advances at the rate of the blended cycle, so a
            // blend that is mostly run steps about as fast as the run does.
            f32 time = mTime * duration;
            if ( not advance( time, duration ) )
                mPlaying = false;
            mTime = time / duration;
        }
        else if ( not advance( mTime, duration ) )
        {
            mPlaying = false;
        }
    }

    if ( mController )
    {
        const ControllerRuntime::Frame frame{ normalized(), previousNormalized, wrapped, InTransition() };
        if ( auto change = mControllerRuntime.Step( *mController, mParameters, frame ) )
        {
            enter( *change );
            resolve();
        }
        // Whatever no transition took this frame is gone.
        mParameters.ResetTriggers();
    }

    for ( Animator::Layer& layer : layers )
        layer.mRatio = normalized();

    if ( mPendingTransition > 0.0f )
    {
        mAnimator->BeginTransition( mPendingTransition );
        mPendingTransition = 0.0f;
    }
    mAnimator->Sample( layers, dt );
    mAnimator->Skin();
}


// The controller's live view: the state, every parameter (editable - a
// tweak while watching, not an edit of the scene), and the transitions out of
// the current state with whether each would fire now.
static void DrawControllerState( AnimatorComponent& component )
{
    const AnimationController& controller = *component.mController;
    const ControllerRuntime& runtime = component.mControllerRuntime;
    const string_view state = component.CurrentState();
    ImGui::Text( "state: %.*s", (int)state.size(), state.data() );

    for ( const auto& [name, declared] : controller.mParameters )
    {
        auto iter = component.mParameters.mValues.find( name );
        if ( iter == component.mParameters.mValues.end() )
            continue;
        Parameter& parameter = iter->second;
        switch ( declared.mType )
        {
        case Parameter::Type::Float:
            ImGui::DragFloat( name.c_str(), &parameter.mValue, 0.01f );
            break;
        case Parameter::Type::Bool:
        {
            bool value = parameter.mValue != 0.0f;
            if ( ImGui::Checkbox( name.c_str(), &value ) )
                parameter.mValue = value ? 1.0f : 0.0f;
            break;
        }
        case Parameter::Type::Trigger:
            if ( ImGui::Button( name.c_str() ) )
                parameter.mValue = 1.0f;
            ImGui::SameLine();
            ImGui::TextDisabled( parameter.mValue != 0.0f ? "(set)" : "trigger" );
            break;
        }
    }

    if ( runtime.mCurrent < 0 )
        return;
    // As the last Step saw it, near enough: the frame is over by now.
    const ControllerRuntime::Frame frame{ 0.0f, 0.0f, false, component.InTransition() };
    for ( const Transition& transition : controller.mTransitions )
    {
        if ( transition.mFrom != Transition::cAnyState and transition.mFrom != runtime.mCurrent )
            continue;
        const string to = transition.mTo == Transition::cReturn ? "return" : controller.mStates[transition.mTo].mName;
        string when;
        for ( const Condition& condition : transition.mConditions )
            when += ( when.empty() ? "" : " and " ) + condition.ToString();
        if ( transition.mExitTime )
            when += std::format( "{}exit {:.2f}", when.empty() ? "" : ", ", *transition.mExitTime );
        const bool conditionsHold = std::ranges::all_of( transition.mConditions,
            [&]( const Condition& c ) { return c.Holds( component.mParameters ); } );
        ImGui::TextColored( conditionsHold ? ImVec4( 0.4f, 1.0f, 0.4f, 1.0f ) : ImVec4( 0.6f, 0.6f, 0.6f, 1.0f ),
                            "%s -> %s  [%s]", transition.mFrom == Transition::cAnyState ? "*" : "", to.c_str(), when.c_str() );
    }
}

void AnimatorComponent::OnComponentDraw( InspectorContext& ctx, const Entity& entity, AnimatorComponent& component )
{
    ImGui::TextColored( TEXT_COLOR, "AnimatorComponent" );

    const ModelComponent* modelComponent = TryGetComponent<ModelComponent>( ctx.mProject, entity );
    const Ref<Model> model = modelComponent ? modelComponent->mModel : nullptr;
    if ( not model or not model->Skinned() )
    {
        ImGui::TextDisabled( "The entity's model has no skeleton" );
        return;
    }

    // The controller, if any: which one, where it is, and what it reads.
    const auto& controller = component.mController;
    ComboProperty<AnimatorComponent>( ctx, entity, "controller", controller,
                                      controller ? controller->mName.c_str() : "None",
                                      ctx.mProject.mLoader.mControllers,
                                      []( const auto& entry ) { return entry.first.stem().string(); },
                                      []( const auto& entry ) { return entry.second; },
                                      []( AnimatorComponent& c, const Ref<AnimationController>& v ) { c.SetController( v ); } );
    if ( controller )
    {
        DrawControllerState( component );
        ImGui::Separator();
    }

    // Choosing a clip restarts it - the same as Play() from a script.
    const string& clip = component.mClip;
    ComboProperty<AnimatorComponent>( ctx, entity, "clip", clip, clip.empty() ? "None" : clip.c_str(),
                                      model->mClips,
                                      []( const auto& c ) { return c->mName; },
                                      []( const auto& c ) { return c->mName; },
                                      []( AnimatorComponent& c, const string& v ) { c.Play( v, 0.2f ); } );

    if ( component.IsBlend() )
    {
        // A blend space is authored by a script (or later, a controller); the
        // inspector shows it and drives its parameter.
        ImGui::TextDisabled( "blend '%s':", component.mClip.c_str() );
        for ( const BlendPoint& point : component.mBlend.mPoints )
            ImGui::TextDisabled( "  %s at %.2f", point.mClip.c_str(), point.mValue );
        const f32 lo = component.mBlend.mPoints.front().mValue;
        const f32 hi = component.mBlend.mPoints.back().mValue;
        EditField<AnimatorComponent>( ctx, entity, "Blend", &AnimatorComponent::mBlendValue,
                                      [&]( f32& v ) { return ImGui::SliderFloat( "Blend", &v, lo, hi ); } );
    }

    DragFloatField<AnimatorComponent>( ctx, entity, "Speed", &AnimatorComponent::mSpeed, 0.01f, -10.0f, 10.0f );
    CheckboxField<AnimatorComponent>( ctx, entity, "Loop", &AnimatorComponent::mLoop );

    // Playback is not an edit: scrubbing and pausing are how a clip is looked
    // at, and neither belongs in the history.
    if ( component.IsBlend() )
    {
        ImGui::SliderFloat( "Phase", &component.mTime, 0.0f, 1.0f, "%.2f" );
    }
    else
    {
        const auto& current = model->FindClip( component.mClip );
        const f32 duration = current ? current->mDuration : 0.0f;
        ImGui::SliderFloat( "Time", &component.mTime, 0.0f, duration, "%.2f s" );
    }
    if ( component.mAnimator and component.mAnimator->InTransition() )
        ImGui::TextDisabled( "transition %.0f%%", 100.0f * component.mAnimator->TransitionProgress() );
    if ( component.mPlaying )
    {
        if ( ImGui::Button( "Pause" ) )
            component.mPlaying = false;
    }
    else if ( ImGui::Button( "Play" ) )
    {
        component.mPlaying = true;
    }
}

void AnimatorComponent::ToJson( json& json, const Project& project, const AnimatorComponent& component )
{
    if ( component.mController )
    {
        auto [relPath, _] = project.mLoader.RelAbsFromProjectPath( component.mController->mPath );
        json["Controller"] = relPath;
    }
    json["Clip"] = component.mClip;
    json["Speed"] = component.mSpeed;
    json["Loop"] = component.mLoop;
    json["Playing"] = component.mPlaying;
    if ( component.IsBlend() )
    {
        auto& points = json["Blend"] = json::array();
        for ( const BlendPoint& point : component.mBlend.mPoints )
            points.push_back( { { "Clip", point.mClip }, { "Value", point.mValue } } );
        json["BlendValue"] = component.mBlendValue;
    }
}

void AnimatorComponent::FromJson( const json& json, Project& project, AnimatorComponent& component )
{
    if ( json.is_null() )
        return;
    if ( json.contains( "Controller" ) )
        component.SetController( project.mLoader.LoadAnimationController( json["Controller"] ) );
    if ( json.contains( "Clip" ) )
        component.mClip = json["Clip"];
    if ( json.contains( "Speed" ) )
        component.mSpeed = json["Speed"];
    if ( json.contains( "Loop" ) )
        component.mLoop = json["Loop"];
    if ( json.contains( "Playing" ) )
        component.mPlaying = json["Playing"];
    if ( json.contains( "Blend" ) )
    {
        component.mBlend = {};
        for ( const auto& point : json["Blend"] )
            component.mBlend.Add( point.value( "Clip", string() ), point.value( "Value", 0.0f ) );
    }
    if ( json.contains( "BlendValue" ) )
        component.mBlendValue = json["BlendValue"];
}

void AnimatorComponent::CreateLuaBinding( sol::state& lua )
{
    lua.new_usertype<AnimatorComponent>(
        "Animator",

        "play",
        sol::overload(
            []( AnimatorComponent& c, string_view clip ) { c.Play( clip ); },
            []( AnimatorComponent& c, string_view clip, f32 transition ) { c.Play( clip, transition ); }
        ),
        // play_blend( "locomotion", { { "idle", 0 }, { "walk", 1.5 }, { "run", 4 } }, 0.2 )
        "play_blend",
        sol::overload(
            []( AnimatorComponent& c, string_view name, const sol::table& points ) { c.PlayBlend( name, BlendSpaceFromLua( points ) ); },
            []( AnimatorComponent& c, string_view name, const sol::table& points, f32 transition ) { c.PlayBlend( name, BlendSpaceFromLua( points ), transition ); }
        ),
        "stop",          &AnimatorComponent::Stop,
        "is_playing",    &AnimatorComponent::IsPlaying,
        "in_transition", &AnimatorComponent::InTransition,
        "is_blend",      &AnimatorComponent::IsBlend,

        // Under a controller: what a script drives, and what it can ask.
        "set",
        sol::overload(
            []( AnimatorComponent& c, string_view name, f32 value ) { c.mParameters.Set( name, value ); },
            []( AnimatorComponent& c, string_view name, bool value ) { c.mParameters.Set( name, value ); }
        ),
        "get",     []( const AnimatorComponent& c, string_view name ) { return c.mParameters.Get( name ); },
        "trigger", []( AnimatorComponent& c, string_view name ) { c.mParameters.Trigger( name ); },
        "state",   []( const AnimatorComponent& c ) { return string( c.CurrentState() ); },
        "controller",
        sol::property( []( const AnimatorComponent& c ) { return c.mController ? c.mController->mPath.generic_string() : string(); } ),

        "clip",    sol::readonly( &AnimatorComponent::mClip ),
        "time",    &AnimatorComponent::mTime,
        "speed",   &AnimatorComponent::mSpeed,
        "loop",    &AnimatorComponent::mLoop,
        "blend",   &AnimatorComponent::mBlendValue,

        sol::meta_function::to_string,
        []( const AnimatorComponent& c ) { return c.mClip.empty() ? "none" : c.mClip; }
    );
}

}
