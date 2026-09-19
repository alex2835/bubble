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

// { "Spine1", "!Neck" } or "Spine1".
vector<string> MaskFromLua( const sol::object& mask )
{
    vector<string> joints;
    if ( mask.is<string>() )
        joints.push_back( mask.as<string>() );
    else if ( mask.is<sol::table>() )
        for ( const auto& [_, joint] : mask.as<sol::table>() )
            joints.push_back( joint.as<string>() );
    else
        throw std::runtime_error( "set_layer: mask is a joint name or an array of them" );
    return joints;
}

// One stream's frame, out of AdvanceStream.
struct StreamStep
{
    vector<Animator::Layer> mLayers;
    // The clip whose events fire, and how the playback moved through it.
    string_view mEventClip;
    f32 mBefore = 0.0f;
    f32 mNow = 0.0f;
    bool mWrapped = false;
};

// Yaw about +Y of a rotation that is (nearly) a yaw.
f32 YawOf( const glm::quat& q )
{
    const vec3 forward = q * vec3( 0.0f, 0.0f, 1.0f );
    return std::atan2( forward.x, forward.z );
}

// Advances `playback` by dt on `model`: time, then the controller's step
// (with `machine`, if any) against `parameters`, then what to sample. An
// overlay (`keepWhileFading`) keeps sampling a clip it was told to stop, so
// its weight has something to fade out on; the base goes to rest at once.
void AdvanceStream( Playback& playback, const StateMachine* machine, Parameters& parameters,
                    const Model& model, f32 dt, bool inTransition, bool keepWhileFading, bool additive,
                    i32 rootJoint, StreamStep& out )
{
    // What the controller says plays, and how: applied on entering a state,
    // and every frame for the values bound to parameters.
    const auto enter = [&]( const ControllerRuntime::Change& change )
    {
        const ControllerState& state = machine->mStates[change.mState];
        if ( state.IsBlend() )
            playback.PlayBlend( state.mName, state.mBlend, change.mDuration );
        else if ( state.IsEmpty() )
            playback.PlayNothing( change.mDuration );
        else
            playback.Play( state.mClip, change.mDuration );
        playback.mLoop = state.IsBlend() or state.mLoop;
        playback.mRootMotion = state.mRootMotion;
        playback.mEnteredState = state.mName;
    };
    if ( machine )
    {
        if ( playback.mRuntime.mCurrent < 0 )
            enter( playback.mRuntime.Enter( *machine, machine->mEntry, 0.0f ) );
        const ControllerState& state = machine->mStates[playback.mRuntime.mCurrent];
        if ( state.IsBlend() )
            playback.mBlendValue = parameters.Get( state.mBlendParameter );
        playback.mSpeed = state.mSpeedParameter.empty() ? state.mSpeed : parameters.Get( state.mSpeedParameter );
    }

    // Advances `time` through a cycle of `length` by this frame, looping or
    // stopping at the ends; false when it stopped.
    bool wrapped = false;
    const auto advance = [&]( f32& time, f32 length )
    {
        time += dt * playback.mSpeed;
        if ( playback.mLoop )
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
    f32 duration = 0.0f;
    const auto resolve = [&]
    {
        out.mLayers.clear();
        duration = 0.0f;
        if ( playback.mFadingOut and not keepWhileFading )
            return;
        if ( playback.IsBlend() )
        {
            for ( const auto& [point, weight] : playback.mBlend.Weights( playback.mBlendValue ) )
            {
                const auto& clip = model.FindClip( playback.mBlend.mPoints[point].mClip );
                if ( clip )
                {
                    duration += weight * clip->mDuration;
                    out.mLayers.push_back( { clip.get(), 0.0f, weight } );
                }
            }
        }
        else if ( const auto& clip = model.FindClip( playback.mClip ) )
        {
            duration = clip->mDuration;
            out.mLayers.push_back( { clip.get(), 0.0f, 1.0f } );
        }
    };
    // Where the playback is, 0..1, for the layers' ratios and the
    // controller's exit times.
    const auto normalized = [&]
    {
        if ( playback.IsBlend() )
            return playback.mTime;
        return duration > 0.0f ? std::clamp( playback.mTime / duration, 0.0f, 1.0f ) : 0.0f;
    };

    resolve();
    const f32 previousNormalized = normalized();
    // The clip whose events fire: the one playing, or the heaviest in a
    // blend. Taken before the advance, since a clip that ends this frame
    // still passes its last markers.
    out.mEventClip = {};
    if ( playback.mFadingOut )
    {
        // Fading out is over; its last markers are not.
    }
    else if ( playback.IsBlend() )
    {
        f32 heaviest = 0.0f;
        for ( const auto& [point, weight] : playback.mBlend.Weights( playback.mBlendValue ) )
            if ( weight > heaviest )
            {
                heaviest = weight;
                out.mEventClip = playback.mBlend.mPoints[point].mClip;
            }
    }
    else
    {
        out.mEventClip = playback.mClip;
    }

    if ( duration > 0.0f and playback.mPlaying )
    {
        if ( playback.IsBlend() )
        {
            // The phase advances at the rate of the blended cycle, so a
            // blend that is mostly run steps about as fast as the run does.
            f32 time = playback.mTime * duration;
            if ( not advance( time, duration ) )
                playback.mPlaying = false;
            playback.mTime = time / duration;
        }
        else if ( not advance( playback.mTime, duration ) )
        {
            playback.mPlaying = false;
        }
    }
    out.mBefore = previousNormalized;
    out.mNow = normalized();
    out.mWrapped = wrapped;

    if ( machine )
    {
        const ControllerRuntime::Frame frame{ out.mNow, previousNormalized, wrapped, inTransition or playback.mPendingTransition > 0.0f };
        if ( auto change = playback.mRuntime.Step( *machine, parameters, frame ) )
        {
            enter( *change );
            resolve();
        }
    }

    for ( Animator::Layer& layer : out.mLayers )
    {
        layer.mRatio = normalized();
        layer.mAdditive = additive;
        if ( playback.mRootMotion and not additive and rootJoint >= 0 )
            layer.mRootMotion = layer.mClip->WithRootMotion( *model.mSkeleton, rootJoint );
    }
}
}


// Playback

void Playback::Play( string_view clip, f32 transition )
{
    mClip = clip;
    mBlend = {};
    mTime = 0.0f;
    mPlaying = true;
    mFadingOut = false;
    mPendingTransition = std::max( transition, 0.0f );
    mLastTransition = mPendingTransition;
}

void Playback::PlayBlend( string_view name, BlendSpace space, f32 transition )
{
    if ( IsBlend() and mClip == name and mBlend == space )
        return;
    mClip = name;
    mBlend = std::move( space );
    mTime = 0.0f;
    mPlaying = true;
    mFadingOut = false;
    mPendingTransition = std::max( transition, 0.0f );
    mLastTransition = mPendingTransition;
}

void Playback::PlayNothing( f32 transition )
{
    // The clip and its time stay, for an overlay to fade out on; only the
    // weight moves, so the track has no transition to ease.
    mFadingOut = true;
    mLastTransition = std::max( transition, 0.0f );
}


// AnimatorComponent

// Out of line for the Scope<Animator>: the header only forward declares it.
AnimatorComponent::AnimatorComponent() = default;
AnimatorComponent::~AnimatorComponent() = default;
AnimatorComponent::AnimatorComponent( AnimatorComponent&& ) noexcept = default;
AnimatorComponent& AnimatorComponent::operator=( AnimatorComponent&& ) noexcept = default;

AnimatorComponent::AnimatorComponent( const AnimatorComponent& other )
    : mBase( other.mBase ),
      mLayers( other.mLayers ),
      mController( other.mController ),
      mParameters( other.mParameters ),
      mEvents( other.mEvents )
{
}

AnimatorComponent& AnimatorComponent::operator=( const AnimatorComponent& other )
{
    if ( this != &other )
    {
        mBase = other.mBase;
        mLayers = other.mLayers;
        mController = other.mController;
        mParameters = other.mParameters;
        mEvents = other.mEvents;
        // The runtime is kept: it is bound to the model, not to the settings,
        // and is replaced by the update if the model changed.
    }
    return *this;
}

bool AnimatorComponent::InTransition() const
{
    if ( mBase.mPendingTransition > 0.0f )
        return true;
    return mAnimator and mAnimator->Track( 0 ).InTransition();
}

OverlayLayer& AnimatorComponent::Layer( string_view name, vector<string> mask, f32 weight, bool additive )
{
    OverlayLayer* layer = FindLayer( name );
    if ( not layer )
    {
        layer = &mLayers.emplace_back();
        layer->mName = name;
        layer->mPlayback.PlayNothing();
    }
    layer->mMask = std::move( mask );
    layer->mWeight = weight;
    layer->mWeightParameter.clear();
    layer->mAdditive = additive;
    return *layer;
}

OverlayLayer* AnimatorComponent::FindLayer( string_view name )
{
    for ( OverlayLayer& layer : mLayers )
        if ( layer.mName == name )
            return &layer;
    return nullptr;
}

void AnimatorComponent::SetController( const Ref<AnimationController>& controller )
{
    mController = controller;
    mBase.mRuntime = {};
    mParameters = controller ? controller->DefaultParameters() : Parameters{};
    if ( not controller )
        return;
    mLayers.clear();
    for ( const ControllerLayer& declared : controller->mLayers )
    {
        OverlayLayer& layer = mLayers.emplace_back();
        layer.mName = declared.mName;
        layer.mMask = declared.mMask;
        layer.mWeight = declared.mWeight;
        layer.mWeightParameter = declared.mWeightParameter;
        layer.mAdditive = declared.mAdditive;
        layer.mPlayback.PlayNothing();
    }
}

string_view AnimatorComponent::CurrentState() const
{
    if ( not mController or mBase.mRuntime.mCurrent < 0 )
        return {};
    return mController->mStates[mBase.mRuntime.mCurrent].mName;
}

void AnimatorComponent::AddEvent( string_view clip, f32 time, string name )
{
    vector<ClipEvent>& list = mEvents[string( clip )];
    list.push_back( { std::clamp( time, 0.0f, 1.0f ), std::move( name ) } );
    std::ranges::stable_sort( list, {}, &ClipEvent::mTime );
}


void AnimatorComponent::Advance( const Ref<Model>& model, f32 dt, const mat4& entityWorld )
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
    Animator& animator = *mAnimator;

    mFiredEvents.clear();
    mBase.mEnteredState.clear();
    for ( OverlayLayer& layer : mLayers )
        layer.mPlayback.mEnteredState.clear();
    mRootDelta = vec3( 0.0f );
    mRootYawDelta = 0.0f;
    if ( mController )
        mRootJoint = mController->mRootJoint;
    const i32 rootJoint = mRootJoint.empty() ? -1 : model->mSkeleton->JointIndex( mRootJoint ).value_or( -1 );
    if ( not mRootJoint.empty() and rootJoint < 0 )
        LogWarning( "Animator: root joint '{}' is not in the skeleton of '{}'", mRootJoint, model->mName );

    // A stream's frame: advance, sample into `pose`, ease through its track.
    // Returns the clip whose events fired, having queued them.
    const auto stream = [&]( Playback& playback, const StateMachine* machine, u32 slot, bool additive, Pose& pose )
    {
        PoseTrack& track = animator.Track( slot );
        StreamStep step;
        AdvanceStream( playback, machine, mParameters, *model, dt, track.InTransition(), slot != 0, additive, rootJoint, step );

        // The travel of the frame, each clip's weighted by its share of the
        // blend. The layer that takes the root motion out is the one that
        // must report it, or the character would stand still for nothing.
        for ( const Animator::Layer& layer : step.mLayers )
        {
            if ( not layer.mRootMotion or layer.mWeight <= 0.0f )
                continue;
            vec3 translation;
            glm::quat rotation;
            layer.mRootMotion->Delta( step.mBefore, step.mNow, step.mWrapped, translation, rotation );
            mRootDelta += translation * layer.mWeight;
            mRootYawDelta += YawOf( rotation ) * layer.mWeight;
        }

        if ( not step.mEventClip.empty() )
        {
            if ( mController )
                CrossedEvents( mController->mEvents, step.mEventClip, step.mBefore, step.mNow, step.mWrapped, mFiredEvents );
            CrossedEvents( mEvents, step.mEventClip, step.mBefore, step.mNow, step.mWrapped, mFiredEvents );
        }

        if ( playback.mPendingTransition > 0.0f )
        {
            track.BeginTransition( playback.mPendingTransition );
            playback.mPendingTransition = 0.0f;
        }
        animator.SamplePose( slot, step.mLayers, pose );
        track.Apply( pose, dt );
    };

    // Poses live across the frame in these; the vectors are stable since
    // nothing is added below.
    static thread_local vector<Pose> poses;
    poses.resize( 1 + mLayers.size() );
    const size_t soaJoints = ( animator.JointCount() + 3 ) / 4;
    for ( Pose& pose : poses )
        if ( pose.size() != soaJoints )
            pose = animator.MakePose();

    stream( mBase, mController.get(), 0, false, poses[0] );

    vector<Animator::Overlay> overlays;
    for ( size_t i = 0; i < mLayers.size(); i++ )
    {
        OverlayLayer& layer = mLayers[i];
        const ControllerLayer* declared = nullptr;
        if ( mController and i < mController->mLayers.size() and mController->mLayers[i].mName == layer.mName )
            declared = &mController->mLayers[i];
        stream( layer.mPlayback, declared ? &declared->mMachine : nullptr, static_cast<u32>( i + 1 ), layer.mAdditive, poses[i + 1] );

        // The shown weight follows the asked one at the pace of the last
        // transition - and the asked one is zero while nothing plays.
        const f32 asked = layer.mWeightParameter.empty() ? layer.mWeight : mParameters.Get( layer.mWeightParameter );
        const f32 target = layer.mPlayback.IsEmpty() ? 0.0f : std::clamp( asked, 0.0f, 1.0f );
        const f32 seconds = layer.mPlayback.mLastTransition;
        if ( seconds <= 0.0f or dt <= 0.0f )
            layer.mShownWeight = target;
        else
            layer.mShownWeight += std::clamp( target - layer.mShownWeight, -dt / seconds, dt / seconds );

        if ( layer.mShownWeight > 0.0f )
            overlays.push_back( { &poses[i + 1], layer.mMask.empty() ? nullptr : &animator.Mask( layer.mMask ),
                                  layer.mShownWeight, layer.mAdditive } );
    }

    if ( mController )
        mParameters.ResetTriggers();

    animator.Compose( poses[0], overlays );

    // IK last, on the composed pose. Targets come in world space; the pose
    // is in the model's, so they go through the entity's inverse.
    const mat4 toModel = glm::inverse( entityWorld );
    const auto joint = [&]( const string& name )
    {
        const auto index = model->mSkeleton->JointIndex( name );
        if ( not index )
            LogWarning( "Animator: IK joint '{}' is not in the skeleton of '{}'", name, model->mName );
        return index ? (i32)*index : -1;
    };
    for ( const AimRequest& aim : mAims )
        animator.AimAt( joint( aim.mJoint ), vec3( toModel * vec4( aim.mTarget, 1.0f ) ), aim.mForward, aim.mUp, aim.mWeight );
    for ( const ReachRequest& reach : mReaches )
    {
        std::optional<vec3> pole;
        if ( reach.mPoleVector )
            pole = vec3( toModel * vec4( *reach.mPoleVector, 0.0f ) );
        animator.ReachTo( joint( reach.mEndJoint ), vec3( toModel * vec4( reach.mTarget, 1.0f ) ),
                          pole ? &*pole : nullptr, reach.mMidAxis ? &*reach.mMidAxis : nullptr,
                          reach.mSoften, reach.mWeight );
    }
    mAims.clear();
    mReaches.clear();

    animator.Skin();
}


// Inspector

namespace
{
// A stream's controls: what plays, speed, loop, scrub, pause. `label`
// suffixes ImGui ids so several streams can sit in one inspector.
// The stream at `slot`: the base, or an overlay.
Playback* PlaybackAt( AnimatorComponent& component, u32 slot )
{
    if ( slot == 0 )
        return &component.mBase;
    if ( slot - 1 < component.mLayers.size() )
        return &component.mLayers[slot - 1].mPlayback;
    return nullptr;
}

void DrawPlayback( InspectorContext& ctx, const Entity& entity, AnimatorComponent& component,
                   Playback& playback, const Model& model, const char* label, Animator* animator, u32 slot )
{
    ImGui::PushID( label );

    // Choosing a clip restarts it - the same as Play() from a script. The
    // stream is found again by slot rather than held: the command may run
    // on the component after an undo has replaced its layers.
    const string clip = playback.IsEmpty() ? string() : playback.mClip;
    ComboProperty<AnimatorComponent>( ctx, entity, "clip", clip, clip.empty() ? "None" : clip.c_str(),
                                      model.mClips,
                                      []( const auto& c ) { return c->mName; },
                                      []( const auto& c ) { return c->mName; },
                                      [slot]( AnimatorComponent& c, const string& v )
    {
        if ( Playback* p = PlaybackAt( c, slot ) )
            p->Play( v, 0.2f );
    } );

    if ( playback.IsBlend() )
    {
        // A blend space is authored by a script or a controller; the
        // inspector shows it and drives its parameter.
        ImGui::TextDisabled( "blend '%s':", playback.mClip.c_str() );
        for ( const BlendPoint& point : playback.mBlend.mPoints )
            ImGui::TextDisabled( "  %s at %.2f", point.mClip.c_str(), point.mValue );
        const f32 lo = playback.mBlend.mPoints.front().mValue;
        const f32 hi = playback.mBlend.mPoints.back().mValue;
        ImGui::SliderFloat( "Blend", &playback.mBlendValue, lo, hi );
    }

    // Playback is not an edit: scrubbing, speed and pausing are how a clip
    // is looked at, and none of it belongs in the history.
    ImGui::DragFloat( "Speed", &playback.mSpeed, 0.01f, -10.0f, 10.0f );
    ImGui::Checkbox( "Loop", &playback.mLoop );
    if ( playback.IsBlend() )
    {
        ImGui::SliderFloat( "Phase", &playback.mTime, 0.0f, 1.0f, "%.2f" );
    }
    else
    {
        const auto& current = model.FindClip( playback.mClip );
        const f32 duration = current ? current->mDuration : 0.0f;
        ImGui::SliderFloat( "Time", &playback.mTime, 0.0f, duration, "%.2f s" );
    }
    if ( animator and animator->Track( slot ).InTransition() )
        ImGui::TextDisabled( "transition %.0f%%", 100.0f * animator->Track( slot ).TransitionProgress() );
    if ( playback.mPlaying )
    {
        if ( ImGui::Button( "Pause" ) )
            playback.mPlaying = false;
    }
    else if ( ImGui::Button( "Play" ) )
    {
        playback.mPlaying = true;
    }
    ImGui::PopID();
}

// A state machine's live view: the state, and the transitions out of it
// with whether each would fire now.
void DrawMachine( const StateMachine& machine, const Playback& playback, const Parameters& parameters )
{
    const ControllerRuntime& runtime = playback.mRuntime;
    if ( runtime.mCurrent < 0 )
        return;
    ImGui::Text( "state: %s", machine.mStates[runtime.mCurrent].mName.c_str() );
    for ( const Transition& transition : machine.mTransitions )
    {
        if ( transition.mFrom != Transition::cAnyState and transition.mFrom != runtime.mCurrent )
            continue;
        const string to = transition.mTo == Transition::cReturn ? "return" : machine.mStates[transition.mTo].mName;
        string when;
        for ( const Condition& condition : transition.mConditions )
            when += ( when.empty() ? "" : " and " ) + condition.ToString();
        if ( transition.mExitTime )
            when += std::format( "{}exit {:.2f}", when.empty() ? "" : ", ", *transition.mExitTime );
        const bool conditionsHold = std::ranges::all_of( transition.mConditions,
            [&]( const Condition& c ) { return c.Holds( parameters ); } );
        ImGui::TextColored( conditionsHold ? ImVec4( 0.4f, 1.0f, 0.4f, 1.0f ) : ImVec4( 0.6f, 0.6f, 0.6f, 1.0f ),
                            "%s-> %s  [%s]", transition.mFrom == Transition::cAnyState ? "* " : "", to.c_str(), when.c_str() );
    }
}

// Every parameter, editable - a tweak while watching, not an edit of the
// scene.
void DrawParameters( AnimatorComponent& component )
{
    for ( const auto& [name, declared] : component.mController->mParameters )
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
        DrawParameters( component );
        DrawMachine( *controller, component.mBase, component.mParameters );
    }
    DrawPlayback( ctx, entity, component, component.mBase, *model, "base", component.mAnimator.get(), 0 );
    if ( component.mBase.mRootMotion )
        ImGui::TextDisabled( "root motion on '%s': %.3f %.3f %.3f, yaw %.3f", component.mRootJoint.c_str(),
                             component.mRootDelta.x, component.mRootDelta.y, component.mRootDelta.z, component.mRootYawDelta );

    for ( size_t i = 0; i < component.mLayers.size(); i++ )
    {
        OverlayLayer& layer = component.mLayers[i];
        ImGui::Separator();
        string mask;
        for ( const string& joint : layer.mMask )
            mask += ( mask.empty() ? "" : ", " ) + joint;
        ImGui::Text( "%slayer '%s' on [%s]  weight %.2f (shown %.2f)", layer.mAdditive ? "additive " : "",
                     layer.mName.c_str(), mask.c_str(), layer.mWeight, layer.mShownWeight );
        if ( controller and i < controller->mLayers.size() )
            DrawMachine( controller->mLayers[i].mMachine, layer.mPlayback, component.mParameters );
        DrawPlayback( ctx, entity, component, layer.mPlayback, *model, layer.mName.c_str(), component.mAnimator.get(), static_cast<u32>( i + 1 ) );
    }
}


// Serialization

namespace
{
void PlaybackToJson( json& j, const Playback& playback )
{
    j["Clip"] = playback.mClip;
    j["Speed"] = playback.mSpeed;
    j["Loop"] = playback.mLoop;
    j["Playing"] = playback.mPlaying;
    if ( playback.IsBlend() )
    {
        auto& points = j["Blend"] = json::array();
        for ( const BlendPoint& point : playback.mBlend.mPoints )
            points.push_back( { { "Clip", point.mClip }, { "Value", point.mValue } } );
        j["BlendValue"] = playback.mBlendValue;
    }
}

void PlaybackFromJson( const json& j, Playback& playback )
{
    if ( j.contains( "Clip" ) )
        playback.mClip = j["Clip"];
    if ( j.contains( "Speed" ) )
        playback.mSpeed = j["Speed"];
    if ( j.contains( "Loop" ) )
        playback.mLoop = j["Loop"];
    if ( j.contains( "Playing" ) )
        playback.mPlaying = j["Playing"];
    if ( j.contains( "Blend" ) )
    {
        playback.mBlend = {};
        for ( const auto& point : j["Blend"] )
            playback.mBlend.Add( point.value( "Clip", string() ), point.value( "Value", 0.0f ) );
    }
    if ( j.contains( "BlendValue" ) )
        playback.mBlendValue = j["BlendValue"];
}
}

void AnimatorComponent::ToJson( json& json, const Project& project, const AnimatorComponent& component )
{
    if ( component.mController )
    {
        auto [relPath, _] = project.mLoader.RelAbsFromProjectPath( component.mController->mPath );
        json["Controller"] = relPath;
    }
    PlaybackToJson( json, component.mBase );
    if ( not component.mController and not component.mRootJoint.empty() )
    {
        json["RootJoint"] = component.mRootJoint;
        json["RootMotion"] = component.mBase.mRootMotion;
    }
    // A controller's layers come back with it; a script's are saved.
    if ( not component.mController and not component.mLayers.empty() )
    {
        auto& layers = json["Layers"] = json::array();
        for ( const OverlayLayer& layer : component.mLayers )
        {
            auto& j = layers.emplace_back();
            j["Name"] = layer.mName;
            j["Mask"] = layer.mMask;
            j["Weight"] = layer.mWeight;
            j["Additive"] = layer.mAdditive;
            PlaybackToJson( j, layer.mPlayback );
        }
    }
}

void AnimatorComponent::FromJson( const json& json, Project& project, AnimatorComponent& component )
{
    if ( json.is_null() )
        return;
    if ( json.contains( "Controller" ) )
        component.SetController( project.mLoader.LoadAnimationController( json["Controller"] ) );
    PlaybackFromJson( json, component.mBase );
    if ( json.contains( "RootJoint" ) )
    {
        component.mRootJoint = json["RootJoint"];
        component.mBase.mRootMotion = json.value( "RootMotion", false );
    }
    if ( json.contains( "Layers" ) and not component.mController )
    {
        component.mLayers.clear();
        for ( const auto& j : json["Layers"] )
        {
            OverlayLayer& layer = component.mLayers.emplace_back();
            layer.mName = j.value( "Name", string() );
            if ( j.contains( "Mask" ) )
                layer.mMask = j["Mask"].get<vector<string>>();
            layer.mWeight = j.value( "Weight", 1.0f );
            layer.mAdditive = j.value( "Additive", false );
            PlaybackFromJson( j, layer.mPlayback );
        }
    }
}


// Lua

void AnimatorComponent::CreateLuaBinding( sol::state& lua )
{
    // The overlay a layer call names; unknown is an error, not a no-op.
    const auto layer = []( AnimatorComponent& c, string_view name ) -> OverlayLayer&
    {
        OverlayLayer* found = c.FindLayer( name );
        if ( not found )
            throw std::runtime_error( std::format( "animator: no layer '{}' - set_layer it first", name ) );
        return *found;
    };

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

        // Overlay layers: set_layer( "upper", { "Spine1" }, 1 ) makes or
        // reshapes one; the play_layer family plays on it.
        "set_layer",
        sol::overload(
            []( AnimatorComponent& c, string_view name, const sol::object& mask ) { c.Layer( name, MaskFromLua( mask ), 1.0f ); },
            []( AnimatorComponent& c, string_view name, const sol::object& mask, f32 weight ) { c.Layer( name, MaskFromLua( mask ), weight ); },
            []( AnimatorComponent& c, string_view name, const sol::object& mask, f32 weight, bool additive ) { c.Layer( name, MaskFromLua( mask ), weight, additive ); }
        ),
        "play_layer",
        sol::overload(
            [layer]( AnimatorComponent& c, string_view name, string_view clip ) { layer( c, name ).mPlayback.Play( clip ); },
            [layer]( AnimatorComponent& c, string_view name, string_view clip, f32 transition ) { layer( c, name ).mPlayback.Play( clip, transition ); }
        ),
        "stop_layer",
        sol::overload(
            [layer]( AnimatorComponent& c, string_view name ) { layer( c, name ).mPlayback.PlayNothing(); },
            [layer]( AnimatorComponent& c, string_view name, f32 transition ) { layer( c, name ).mPlayback.PlayNothing( transition ); }
        ),
        "layer_weight",
        sol::overload(
            [layer]( AnimatorComponent& c, string_view name ) { return layer( c, name ).mWeight; },
            [layer]( AnimatorComponent& c, string_view name, f32 weight ) { layer( c, name ).mWeight = weight; layer( c, name ).mWeightParameter.clear(); }
        ),
        "layer_clip",  [layer]( AnimatorComponent& c, string_view name ) { return layer( c, name ).mPlayback.mClip; },
        "layer_state",
        [layer]( AnimatorComponent& c, string_view name ) -> string
        {
            const OverlayLayer& l = layer( c, name );
            if ( not c.mController or l.mPlayback.mRuntime.mCurrent < 0 )
                return {};
            for ( const ControllerLayer& declared : c.mController->mLayers )
                if ( declared.mName == l.mName )
                    return declared.mMachine.mStates[l.mPlayback.mRuntime.mCurrent].mName;
            return {};
        },
        "layer_playing", [layer]( AnimatorComponent& c, string_view name ) { const auto& p = layer( c, name ).mPlayback; return p.mPlaying and not p.IsEmpty(); },

        // Under a controller: what a script drives, and what it can ask.
        "set",
        sol::overload(
            []( AnimatorComponent& c, string_view name, f32 value ) { c.mParameters.Set( name, value ); },
            []( AnimatorComponent& c, string_view name, bool value ) { c.mParameters.Set( name, value ); }
        ),
        "get",     []( const AnimatorComponent& c, string_view name ) { return c.mParameters.Get( name ); },
        "trigger", []( AnimatorComponent& c, string_view name ) { c.mParameters.Trigger( name ); },
        "state",   []( const AnimatorComponent& c ) { return string( c.CurrentState() ); },
        // What the last animation update produced. Scripts run before it,
        // so these are the previous frame's - a footstep is heard a frame
        // late, which is 16ms, not a problem.
        "events",  []( const AnimatorComponent& c ) { return sol::as_table( c.mFiredEvents ); },
        "entered_state",
        []( const AnimatorComponent& c ) -> sol::optional<string>
        {
            if ( c.mBase.mEnteredState.empty() )
                return sol::nullopt;
            return c.mBase.mEnteredState;
        },
        "add_event", []( AnimatorComponent& c, string_view clip, f32 time, string name ) { c.AddEvent( clip, time, std::move( name ) ); },
        // Root motion: root_motion( "Hips" ) takes the joint's travel out of
        // the base's clips from then on; root_delta() and root_yaw_delta()
        // are what it travelled during the last update, in the model's
        // space, for the script to apply.
        "root_motion",
        sol::overload(
            []( AnimatorComponent& c, string_view joint ) { c.mRootJoint = joint; c.mBase.mRootMotion = not joint.empty(); },
            []( AnimatorComponent& c ) { c.mBase.mRootMotion = false; }
        ),
        "root_delta",     []( const AnimatorComponent& c ) { return c.mRootDelta; },
        "root_yaw_delta", []( const AnimatorComponent& c ) { return c.mRootYawDelta; },

        // IK for this frame, world space targets. look_at( joint, target,
        // { weight, forward, up } ) turns a joint at a point; reach(
        // end_joint, target, { weight, pole, mid_axis, soften } ) bends the
        // two bones above the end joint to put it on the point.
        "look_at",
        sol::overload(
            []( AnimatorComponent& c, string_view joint, const vec3& target )
            {
                c.mAims.push_back( { string( joint ), target } );
            },
            []( AnimatorComponent& c, string_view joint, const vec3& target, const sol::table& options )
            {
                AnimatorComponent::AimRequest aim{ string( joint ), target };
                aim.mWeight = options.get_or( "weight", 1.0f );
                aim.mForward = options.get_or( "forward", aim.mForward );
                aim.mUp = options.get_or( "up", aim.mUp );
                c.mAims.push_back( std::move( aim ) );
            }
        ),
        "reach",
        sol::overload(
            []( AnimatorComponent& c, string_view endJoint, const vec3& target )
            {
                c.mReaches.push_back( { string( endJoint ), target } );
            },
            []( AnimatorComponent& c, string_view endJoint, const vec3& target, const sol::table& options )
            {
                AnimatorComponent::ReachRequest reach{ string( endJoint ), target };
                reach.mWeight = options.get_or( "weight", 1.0f );
                reach.mSoften = options.get_or( "soften", 0.97f );
                if ( const sol::optional<vec3> pole = options["pole"] )
                    reach.mPoleVector = *pole;
                if ( const sol::optional<vec3> axis = options["mid_axis"] )
                    reach.mMidAxis = *axis;
                c.mReaches.push_back( std::move( reach ) );
            }
        ),
        "controller",
        sol::property( []( const AnimatorComponent& c ) { return c.mController ? c.mController->mPath.generic_string() : string(); } ),

        "clip",    sol::property( []( const AnimatorComponent& c ) { return c.mBase.mClip; } ),
        "time",    sol::property( []( const AnimatorComponent& c ) { return c.mBase.mTime; },
                                  []( AnimatorComponent& c, f32 v ) { c.mBase.mTime = v; } ),
        "speed",   sol::property( []( const AnimatorComponent& c ) { return c.mBase.mSpeed; },
                                  []( AnimatorComponent& c, f32 v ) { c.mBase.mSpeed = v; } ),
        "loop",    sol::property( []( const AnimatorComponent& c ) { return c.mBase.mLoop; },
                                  []( AnimatorComponent& c, bool v ) { c.mBase.mLoop = v; } ),
        "blend",   sol::property( []( const AnimatorComponent& c ) { return c.mBase.mBlendValue; },
                                  []( AnimatorComponent& c, f32 v ) { c.mBase.mBlendValue = v; } ),

        sol::meta_function::to_string,
        []( const AnimatorComponent& c ) { return c.mBase.mClip.empty() ? "none" : c.mBase.mClip; }
    );
}

}
