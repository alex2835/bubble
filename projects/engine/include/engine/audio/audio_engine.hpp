#pragma once
#include "engine/audio/sound.hpp"
#include "engine/types/number.hpp"
#include "engine/types/glm.hpp"
#include "engine/types/map.hpp"
#include "engine/types/pointer.hpp"

struct ma_engine;
struct ma_sound;

namespace bubble
{
// One playing instance of a Sound, owned by the AudioEngine.
//
// Components hold this handle rather than a ma_sound of their own. ma_sound is
// a node in miniaudio's graph and stores pointers back into itself and into its
// neighbours, so it can be neither copied nor moved - and the ECS does both to
// components freely, on every pool growth.
//
// Copying a handle deliberately yields an invalid one. A copied AudioSource is
// a second source with the same settings, not a second owner of one playing
// voice; the alternative - two components that both believe they may stop the
// same voice - is a use after free waiting for a scene duplicate.
struct VoiceHandle
{
    static constexpr u64 INVALID_VOICE = 0;

    VoiceHandle() = default;
    explicit VoiceHandle( u64 id ) : mId( id ) {}

    VoiceHandle( const VoiceHandle& ) {}
    VoiceHandle& operator=( const VoiceHandle& ) { mId = INVALID_VOICE; return *this; }

    VoiceHandle( VoiceHandle&& other ) noexcept : mId( other.mId ) { other.mId = INVALID_VOICE; }
    VoiceHandle& operator=( VoiceHandle&& other ) noexcept
    {
        if ( this != &other )
        {
            mId = other.mId;
            other.mId = INVALID_VOICE;
        }
        return *this;
    }

    bool IsValid() const { return mId != INVALID_VOICE; }
    u64 Id() const { return mId; }
    void Clear() { mId = INVALID_VOICE; }

private:
    u64 mId = INVALID_VOICE;
};


// How one voice is mixed and positioned. Held by value on AudioSourceComponent
// and pushed into miniaudio whenever it changes, so the component stays plain
// serialisable data and the engine keeps the live object.
struct VoiceParams
{
    f32 mVolume = 1.0f;
    f32 mPitch = 1.0f;
    bool mLooping = false;
    // Off makes the voice 2D - full volume regardless of where the listener is.
    // What UI clicks and music want.
    bool mSpatialized = true;
    // No attenuation closer than this, silence past the far one.
    f32 mMinDistance = 1.0f;
    f32 mMaxDistance = 100.0f;
    f32 mRolloff = 1.0f;
    vec3 mPosition = vec3( 0.0f );
};


class AudioEngine
{
public:
    AudioEngine();
    AudioEngine( const AudioEngine& ) = delete;
    AudioEngine& operator=( const AudioEngine& ) = delete;
    ~AudioEngine();

    // Mirrors ScriptingEngine::GlobalState(). Components start and stop voices
    // from their inspector callbacks and from the Lua bindings, and neither is
    // handed an engine reference - threading one through every component hook
    // to reach a process wide device is worse than naming it here.
    // Throws if no AudioEngine is alive.
    static AudioEngine& Get();
    static bool Exists();

    VoiceHandle Play( const Ref<Sound>& sound, const VoiceParams& params );
    void Stop( VoiceHandle& voice );
    void StopAll();

    bool IsPlaying( const VoiceHandle& voice ) const;
    void SetVoicePosition( const VoiceHandle& voice, const vec3& position );
    void ApplyVoiceParams( const VoiceHandle& voice, const VoiceParams& params );

    // Forward and up come from the listener entity's transform. miniaudio
    // supports several listeners; the engine drives listener 0 and leaves the
    // rest for whoever adds split screen.
    void SetListener( const vec3& position, const vec3& forward, const vec3& up );

    f32 GetMasterVolume() const;
    void SetMasterVolume( f32 volume );

    // Reaps voices that have run to completion. A one shot sound holds a
    // ma_sound and a resource manager reference until something notices it
    // finished, and nothing else ever will - miniaudio does not free voices the
    // caller started. Called once a frame from Engine::OnUpdate.
    void OnUpdate();

    // Browsers refuse to start an AudioContext until the page has handled a
    // user gesture, so under Emscripten the device comes up suspended and every
    // sound plays into nothing.
    //
    // Driven from OnUpdate rather than from an input handler: resume() succeeds
    // on any frame after the page has seen a gesture, and tying it to one
    // specific key or click means whoever adds a new way to start the game gets
    // silence. Costs one cached bool per frame once it has taken, and is a
    // no-op off the web.
    void TryResumeAudioContext();

    ma_engine* Handle() { return mEngine.get(); }

private:
    struct Voice
    {
        Scope<ma_sound> mSound;
        // Kept alive for as long as the voice plays. The resource manager
        // caches the PCM, but the asset record is what the editor and the
        // serialiser hold, and a voice outliving it would leave the inspector
        // showing a sound nothing can name.
        Ref<Sound> mAsset;
    };

    static AudioEngine* sInstance;

    Scope<ma_engine> mEngine;
    hash_map<u64, Voice> mVoices;
    // Never reused, so a stale handle to a finished voice reads as finished
    // rather than as whatever started next in its slot.
    u64 mNextVoiceId = 1;
    bool mInitialized = false;
    bool mAudioContextRunning = false;

    Voice* FindVoice( u64 id );
    const Voice* FindVoice( u64 id ) const;
};

}
