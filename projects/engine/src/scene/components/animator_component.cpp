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
      mPlaying( other.mPlaying )
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
        // The runtime is kept: it is bound to the model, not to the settings,
        // and is replaced by the update if the model changed.
    }
    return *this;
}

void AnimatorComponent::Play( string_view clip, f32 transition )
{
    mClip = clip;
    mTime = 0.0f;
    mPlaying = true;
    mPendingTransition = std::max( transition, 0.0f );
}

bool AnimatorComponent::InTransition() const
{
    return mPendingTransition > 0.0f or ( mAnimator and mAnimator->InTransition() );
}

void AnimatorComponent::Stop()
{
    mPlaying = false;
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

    DragFloatField<AnimatorComponent>( ctx, entity, "Speed", &AnimatorComponent::mSpeed, 0.01f, -10.0f, 10.0f );
    CheckboxField<AnimatorComponent>( ctx, entity, "Loop", &AnimatorComponent::mLoop );

    // Playback is not an edit: scrubbing and pausing are how a clip is looked
    // at, and neither belongs in the history.
    const auto& current = model->FindClip( component.mClip );
    const f32 duration = current ? current->mDuration : 0.0f;
    ImGui::SliderFloat( "Time", &component.mTime, 0.0f, duration, "%.2f s" );
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
        "stop",          &AnimatorComponent::Stop,
        "is_playing",    &AnimatorComponent::IsPlaying,
        "in_transition", &AnimatorComponent::InTransition,

        "clip",    sol::readonly( &AnimatorComponent::mClip ),
        "time",    &AnimatorComponent::mTime,
        "speed",   &AnimatorComponent::mSpeed,
        "loop",    &AnimatorComponent::mLoop,

        sol::meta_function::to_string,
        []( const AnimatorComponent& c ) { return c.mClip.empty() ? "none" : c.mClip; }
    );
}

}
