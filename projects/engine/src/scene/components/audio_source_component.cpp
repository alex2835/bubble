#include "engine/pch/pch.hpp"
#include "engine/reflection/reflection.hpp"
#include "engine/scene/components/audio_source_component.hpp"
#include "engine/scene/components/component_draw_utils.hpp"
#include "engine/project/project.hpp"
#include "engine/utils/imgui_utils.hpp"
#include "engine/serialization/types_serialization.hpp"
#include "engine/types/string.hpp"
#include <nlohmann/json.hpp>
#include <sol/sol.hpp>

namespace bubble
{
namespace
{
// A voice setting as a property of the component.
template <auto Member>
auto Param( const AudioSourceComponent& c )
{
    return c.mParams.*Member;
}

template <auto Member>
void SetParam( AudioSourceComponent& c, std::remove_cvref_t<decltype( std::declval<VoiceParams>().*Member )> value )
{
    c.mParams.*Member = value;
}

Ref<Sound> SoundOf( const AudioSourceComponent& c ) { return c.mSound; }
// A new sound ends the one playing.
void SetSound( AudioSourceComponent& c, Ref<Sound> sound )
{
    c.Stop();
    c.mSound = std::move( sound );
}

void Changed( AudioSourceComponent& c )
{
    // Only on a change: pushing every setting into miniaudio every frame would
    // contend with the audio thread for nothing.
    c.ApplyParams();
}

bool Spatialized( const entt::meta_any& c ) { return c.cast<const AudioSourceComponent&>().mParams.mSpatialized; }
}

void AudioSourceComponent::Reflect()
{
    TypeBuilder<AudioSourceComponent>( Name().data() )
        .Note( "Position comes from this entity's transform." )
        .Property<&SetSound, &SoundOf>( "sound" )
        .Property<&SetParam<&VoiceParams::mVolume>, &Param<&VoiceParams::mVolume>>( "volume", {
            .mMin = 0.0f, .mMax = 2.0f, .mFlags = FieldInfo::Slider } )
        .Property<&SetParam<&VoiceParams::mPitch>, &Param<&VoiceParams::mPitch>>( "pitch", {
            .mMin = 0.1f, .mMax = 4.0f, .mFlags = FieldInfo::Slider } )
        .Property<&SetParam<&VoiceParams::mLooping>, &Param<&VoiceParams::mLooping>>( "looping" )
        .Property<&SetParam<&VoiceParams::mSpatialized>, &Param<&VoiceParams::mSpatialized>>( "spatialized", {
            .mTooltip = "Off: full volume wherever the listener is - for music and UI." } )
        .Property<&SetParam<&VoiceParams::mMinDistance>, &Param<&VoiceParams::mMinDistance>>( "min_distance", {
            .mMin = 0.0f, .mMax = 10000.0f, .mSpeed = 0.1f, .mVisible = Spatialized } )
        .Property<&SetParam<&VoiceParams::mMaxDistance>, &Param<&VoiceParams::mMaxDistance>>( "max_distance", {
            .mMin = 0.0f, .mMax = 10000.0f, .mSpeed = 0.1f, .mVisible = Spatialized } )
        .Property<&SetParam<&VoiceParams::mRolloff>, &Param<&VoiceParams::mRolloff>>( "rolloff", {
            .mMin = 0.0f, .mMax = 4.0f, .mFlags = FieldInfo::Slider, .mVisible = Spatialized } )
        .Field<&AudioSourceComponent::mPlayOnStart>( "play_on_start" )
        .OnChanged<&Changed>();
}

void AudioSourceComponent::BindLuaMethods( sol::state&, sol::usertype<AudioSourceComponent>& type )
{
    type["play"] = &AudioSourceComponent::Play;
    type["stop"] = &AudioSourceComponent::Stop;
    type["is_playing"] = &AudioSourceComponent::IsPlaying;
    type[sol::meta_function::to_string] = []( const AudioSourceComponent& c ) { return c.mSound ? c.mSound->mName : "null"; };
}

// Auditioning is not an edit.
void AudioSourceComponent::DrawExtras( InspectorContext&, const Entity&, AudioSourceComponent& component )
{
    if ( component.IsPlaying() )
    {
        if ( ImGui::Button( "Stop" ) )
            component.Stop();
    }
    else if ( ImGui::Button( "Play" ) )
    {
        component.Play();
    }
}

AudioSourceComponent::AudioSourceComponent( const Ref<Sound>& sound )
    : mSound( sound )
{
}

AudioSourceComponent::~AudioSourceComponent()
{
    Stop();
}

AudioSourceComponent::AudioSourceComponent( AudioSourceComponent&& other ) noexcept
    : mSound( std::move( other.mSound ) ),
      mParams( other.mParams ),
      mPlayOnStart( other.mPlayOnStart ),
      mVoice( std::move( other.mVoice ) )
{
}

AudioSourceComponent& AudioSourceComponent::operator=( AudioSourceComponent&& other ) noexcept
{
    if ( this != &other )
    {
        // Whatever this component was playing is being overwritten. Stopping it
        // first is what keeps the voice pool from filling with sounds that no
        // component can reach any more.
        Stop();
        mSound = std::move( other.mSound );
        mParams = other.mParams;
        mPlayOnStart = other.mPlayOnStart;
        mVoice = std::move( other.mVoice );
    }
    return *this;
}

void AudioSourceComponent::Play()
{
    if ( not AudioEngine::Exists() )
        return;

    Stop();
    mVoice = AudioEngine::Get().Play( mSound, mParams );
}

void AudioSourceComponent::Stop()
{
    // Checked rather than assumed: components are destroyed during engine
    // shutdown and by the scene reset in OnEnd, and by then the AudioEngine may
    // already be gone.
    if ( mVoice.IsValid() and AudioEngine::Exists() )
        AudioEngine::Get().Stop( mVoice );
    else
        mVoice.Clear();
}

void AudioSourceComponent::SyncToTransform( const TransformComponent& transform )
{
    mParams.mPosition = transform.World().mPosition;

    // Only a spatialized voice that is actually playing: a source whose sound
    // has finished has no voice to move, and the lookup would be pure overhead.
    if ( mParams.mSpatialized and mVoice.IsValid() and AudioEngine::Exists() )
        AudioEngine::Get().SetVoicePosition( mVoice, transform.World().mPosition );
}

bool AudioSourceComponent::IsPlaying() const
{
    return AudioEngine::Exists() and AudioEngine::Get().IsPlaying( mVoice );
}

void AudioSourceComponent::ApplyParams()
{
    if ( mVoice.IsValid() and AudioEngine::Exists() )
        AudioEngine::Get().ApplyVoiceParams( mVoice, mParams );
}


}
