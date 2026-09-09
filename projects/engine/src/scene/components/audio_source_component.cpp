#include "engine/pch/pch.hpp"
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
    mParams.mPosition = transform.mPosition;

    // Only a spatialized voice that is actually playing: a source whose sound
    // has finished has no voice to move, and the lookup would be pure overhead.
    if ( mParams.mSpatialized and mVoice.IsValid() and AudioEngine::Exists() )
        AudioEngine::Get().SetVoicePosition( mVoice, transform.mPosition );
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


void AudioSourceComponent::OnComponentDraw( const Project& project, const Entity& entity, AudioSourceComponent& component )
{
    ImGui::TextColored( TEXT_COLOR, "AudioSourceComponent" );

    const auto& sound = component.mSound;
    auto soundName = sound ? sound->mName.c_str() : "Not selected";
    if ( ImGui::BeginCombo( "sounds", soundName ) )
    {
        for ( const auto& [soundPath, loadedSound] : project.mLoader.mSounds )
        {
            auto soundComboName = soundPath.stem().string();
            if ( ImGui::Selectable( soundComboName.c_str(), soundComboName == soundName ) )
            {
                component.Stop();
                component.mSound = loadedSound;
            }
        }
        ImGui::EndCombo();
    }

    bool changed = false;
    changed |= ImGui::SliderFloat( "Volume", &component.mParams.mVolume, 0.0f, 2.0f );
    changed |= ImGui::SliderFloat( "Pitch", &component.mParams.mPitch, 0.1f, 4.0f );
    changed |= ImGui::Checkbox( "Looping", &component.mParams.mLooping );
    changed |= ImGui::Checkbox( "Spatialized", &component.mParams.mSpatialized );
    ImGui::Checkbox( "Play on start", &component.mPlayOnStart );

    if ( component.mParams.mSpatialized )
    {
        ImGui::Indent();
        changed |= ImGui::DragFloat( "Min distance", &component.mParams.mMinDistance, 0.1f, 0.0f, 10000.0f );
        changed |= ImGui::DragFloat( "Max distance", &component.mParams.mMaxDistance, 0.1f, 0.0f, 10000.0f );
        changed |= ImGui::SliderFloat( "Rolloff", &component.mParams.mRolloff, 0.0f, 4.0f );
        ImGui::Unindent();
    }

    // Only when something moved: pushing every field into miniaudio on every
    // inspector frame would contend with the audio thread for no reason.
    if ( changed )
        component.ApplyParams();

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

void AudioSourceComponent::ToJson( json& json, const Project& project, const AudioSourceComponent& component )
{
    if ( component.mSound )
    {
        auto [relPath, _] = project.mLoader.RelAbsFromProjectPath( component.mSound->mPath );
        json["Path"] = relPath;
    }
    json["Volume"] = component.mParams.mVolume;
    json["Pitch"] = component.mParams.mPitch;
    json["Looping"] = component.mParams.mLooping;
    json["Spatialized"] = component.mParams.mSpatialized;
    json["MinDistance"] = component.mParams.mMinDistance;
    json["MaxDistance"] = component.mParams.mMaxDistance;
    json["Rolloff"] = component.mParams.mRolloff;
    json["PlayOnStart"] = component.mPlayOnStart;
}

void AudioSourceComponent::FromJson( const json& json, Project& project, AudioSourceComponent& component )
{
    if ( json.is_null() )
        return;

    if ( json.contains( "Path" ) )
        component.mSound = project.mLoader.LoadSound( json["Path"] );

    if ( json.contains( "Volume" ) )
        component.mParams.mVolume = json["Volume"];
    if ( json.contains( "Pitch" ) )
        component.mParams.mPitch = json["Pitch"];
    if ( json.contains( "Looping" ) )
        component.mParams.mLooping = json["Looping"];
    if ( json.contains( "Spatialized" ) )
        component.mParams.mSpatialized = json["Spatialized"];
    if ( json.contains( "MinDistance" ) )
        component.mParams.mMinDistance = json["MinDistance"];
    if ( json.contains( "MaxDistance" ) )
        component.mParams.mMaxDistance = json["MaxDistance"];
    if ( json.contains( "Rolloff" ) )
        component.mParams.mRolloff = json["Rolloff"];
    if ( json.contains( "PlayOnStart" ) )
        component.mPlayOnStart = json["PlayOnStart"];
}

void AudioSourceComponent::CreateLuaBinding( sol::state& lua )
{
    lua.new_usertype<AudioSourceComponent>(
        "AudioSource",

        "play",       &AudioSourceComponent::Play,
        "stop",       &AudioSourceComponent::Stop,
        "is_playing", &AudioSourceComponent::IsPlaying,

        // Properties write straight through to the playing voice. A script that
        // ramps volume over a few frames should not also have to remember to
        // push the change.
        "volume",
        sol::property(
            []( const AudioSourceComponent& c ) { return c.mParams.mVolume; },
            []( AudioSourceComponent& c, f32 v ) { c.mParams.mVolume = v; c.ApplyParams(); }
        ),
        "pitch",
        sol::property(
            []( const AudioSourceComponent& c ) { return c.mParams.mPitch; },
            []( AudioSourceComponent& c, f32 v ) { c.mParams.mPitch = v; c.ApplyParams(); }
        ),
        "looping",
        sol::property(
            []( const AudioSourceComponent& c ) { return c.mParams.mLooping; },
            []( AudioSourceComponent& c, bool v ) { c.mParams.mLooping = v; c.ApplyParams(); }
        ),
        "spatialized",
        sol::property(
            []( const AudioSourceComponent& c ) { return c.mParams.mSpatialized; },
            []( AudioSourceComponent& c, bool v ) { c.mParams.mSpatialized = v; c.ApplyParams(); }
        ),
        "min_distance",
        sol::property(
            []( const AudioSourceComponent& c ) { return c.mParams.mMinDistance; },
            []( AudioSourceComponent& c, f32 v ) { c.mParams.mMinDistance = v; c.ApplyParams(); }
        ),
        "max_distance",
        sol::property(
            []( const AudioSourceComponent& c ) { return c.mParams.mMaxDistance; },
            []( AudioSourceComponent& c, f32 v ) { c.mParams.mMaxDistance = v; c.ApplyParams(); }
        ),
        "rolloff",
        sol::property(
            []( const AudioSourceComponent& c ) { return c.mParams.mRolloff; },
            []( AudioSourceComponent& c, f32 v ) { c.mParams.mRolloff = v; c.ApplyParams(); }
        ),
        "play_on_start", &AudioSourceComponent::mPlayOnStart,

        sol::meta_function::to_string,
        []( const AudioSourceComponent& c ) { return c.mSound ? c.mSound->mName : "null"; }
    );
}

}
