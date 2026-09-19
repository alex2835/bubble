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
      mBlendValue( other.mBlendValue )
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

    // Advances `time` through a cycle of `length` by this frame, looping or
    // stopping at the ends; false when it stopped.
    const auto advance = [&]( f32& time, f32 length )
    {
        time += dt * mSpeed;
        if ( mLoop )
        {
            time = std::fmod( time, length );
            if ( time < 0.0f )
                time += length;
            return true;
        }
        if ( time < length and time > 0.0f )
            return true;
        // Reached either end - the speed may be negative.
        time = std::clamp( time, 0.0f, length );
        return false;
    };

    vector<Animator::Layer> layers;
    if ( IsBlend() )
    {
        // The phase advances at the rate of the blended cycle, so a blend
        // that is mostly run steps about as fast as the run does.
        const auto weights = mBlend.Weights( mBlendValue );
        f32 duration = 0.0f;
        for ( const auto& [point, weight] : weights )
        {
            const auto& clip = model->FindClip( mBlend.mPoints[point].mClip );
            if ( clip )
            {
                duration += weight * clip->mDuration;
                layers.push_back( { clip.get(), 0.0f, weight } );
            }
        }
        if ( duration > 0.0f and mPlaying )
        {
            // In normalized time: dt seconds is dt / duration of the cycle.
            f32 time = mTime * duration;
            if ( not advance( time, duration ) )
                mPlaying = false;
            mTime = time / duration;
        }
        for ( Animator::Layer& layer : layers )
            layer.mRatio = mTime;
    }
    else
    {
        const AnimationClip* clip = model->FindClip( mClip ).get();
        if ( clip and mPlaying and not advance( mTime, clip->mDuration ) )
            mPlaying = false;
        if ( clip )
            layers.push_back( { clip, clip->mDuration > 0.0f ? mTime / clip->mDuration : 0.0f, 1.0f } );
    }

    if ( mPendingTransition > 0.0f )
    {
        mAnimator->BeginTransition( mPendingTransition );
        mPendingTransition = 0.0f;
    }
    mAnimator->Sample( layers, dt );
    mAnimator->Skin();
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
