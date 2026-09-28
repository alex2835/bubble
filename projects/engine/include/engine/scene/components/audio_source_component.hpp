#pragma once
#include "engine/scene/components/component_base.hpp"
#include "engine/scene/components/transform_component.hpp"
#include "engine/audio/audio_engine.hpp"
#include <sol/sol.hpp>

namespace bubble
{
struct AudioSourceComponent
{
    static int ID() { return static_cast<int>( ComponentID::AudioSource ); }
    static string_view Name() { return "audio_source"sv; }

    // Fields for engine/reflection: saved, shown, set by path and bound to Lua.
    // The voice's settings are flat properties over mParams; a set pushes
    // them into a playing voice.
    static void Reflect();
    // Lua: the fields come from Reflect(); these are what is added to them.
    static void BindLuaMethods( sol::state& lua, sol::usertype<AudioSourceComponent>& type );
    // Under the fields in the inspector: play and stop, to audition.
    static void DrawExtras( InspectorContext& ctx, const Entity& entity, AudioSourceComponent& component );

public:
    AudioSourceComponent() = default;
    explicit AudioSourceComponent( const Ref<Sound>& sound );
    ~AudioSourceComponent();

    // Copying carries the settings and not the voice - see VoiceHandle.
    //
    // The scene moves a component when another of its type is removed (see
    // Scene); a move carries the voice over. The ma_sound itself lives
    // in the AudioEngine and not in here: it points back into itself and
    // cannot move at all.
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
