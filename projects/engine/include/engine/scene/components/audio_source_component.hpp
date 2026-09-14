#pragma once
#include "engine/scene/components/component_base.hpp"
#include "engine/scene/components/transform_component.hpp"
#include "engine/audio/audio_engine.hpp"

namespace bubble
{
struct AudioSourceComponent
{
    static int ID() { return static_cast<int>( ComponentID::AudioSource ); }
    static string_view Name() { return "AudioSource"sv; }

    static void OnComponentDraw( EditContext& ctx, const Entity& entity, AudioSourceComponent& component );
    static void ToJson( json& json, const Project& project, const AudioSourceComponent& component );
    static void FromJson( const json& json, Project& project, AudioSourceComponent& component );
    static void CreateLuaBinding( sol::state& lua );

public:
    AudioSourceComponent() = default;
    explicit AudioSourceComponent( const Ref<Sound>& sound );
    ~AudioSourceComponent();

    // Copying carries the settings and not the voice - see VoiceHandle.
    //
    // Note that recs relocates components with memmove and requires them to be
    // trivially relocatable (recs/pool.hpp), so neither of the move operations
    // below runs when a pool grows - the handle is carried bitwise, which is
    // what we want. They exist for explicit moves in engine code. This is also
    // why the ma_sound itself lives in the AudioEngine and not in here: a
    // ma_sound points back into itself, and memmove would quietly corrupt it.
    AudioSourceComponent( const AudioSourceComponent& ) = default;
    AudioSourceComponent& operator=( const AudioSourceComponent& ) = default;
    AudioSourceComponent( AudioSourceComponent&& ) noexcept;
    AudioSourceComponent& operator=( AudioSourceComponent&& ) noexcept;

    // Restarts from the beginning if this source is already playing - one
    // AudioSource is one voice. Overlapping shots of the same sound are what
    // play_sound() in the Lua API is for.
    void Play();
    void Stop();
    bool IsPlaying() const;

    // Pushes mParams into a live voice. Position is driven by the engine from
    // the entity's TransformComponent, so it is not read from here.
    void ApplyParams();

    // Takes the position from the entity's transform, and moves a live voice
    // there. Play() starts a voice at mParams.mPosition, so this is called
    // where the component is created as well as every frame - without it a
    // sound played on a freshly created entity starts at the origin.
    void SyncToTransform( const TransformComponent& transform );

    Ref<Sound> mSound;
    VoiceParams mParams;
    bool mPlayOnStart = false;

    // Not serialised and not copied: a voice belongs to one live component in
    // one running scene.
    VoiceHandle mVoice;
};

}
