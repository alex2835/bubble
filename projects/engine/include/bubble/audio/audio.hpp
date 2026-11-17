#pragma once
#include "bubble/assets/asset_id.hpp"
#include "bubble/core/name.hpp"
#include "bubble/scene/entity.hpp"
#include "bubble/types/containers.hpp"
#include "bubble/types/math.hpp"
#include "bubble/types/pointer.hpp"
#include "bubble/types/string.hpp"
#include "bubble/world/module.hpp"

namespace bubble
{
class AssetRegistry;

// ---- sounds -------------------------------------------------------------------

// A sound's samples as a voice reads them: a short one decoded whole when it
// loads, a long one - music - kept as its file and decoded as it plays.
struct SoundData
{
    u32 mChannels = 0;
    u32 mSampleRate = 0;
    f64 mSeconds = 0;
    // Interleaved f32: a decoded sound.
    vector<f32> mFrames;
    // The file as it is: a streamed one.
    string mEncoded;
};

// A .wav, .flac or .mp3 as the registry keeps it. Its data is shared: a
// voice playing it holds it, so a reload does not take it from under the
// voice.
struct SoundAsset
{
    Ref<const SoundData> mData;
};

constexpr string_view cSoundKind = "sound";

struct SoundInfo
{
    u32 mChannels = 0;
    u32 mSampleRate = 0;
    f64 mSeconds = 0;
};

// What a file holds, if it is a sound miniaudio decodes: what the importer
// checks a file with.
expected<SoundInfo, string> TryProbeSound( string_view bytes );
// The import cache's form of a sound: whether it streams, and the file.
string WriteSoundAsset( string_view file, bool stream );
// Teaches the registry sounds: <id>.sound in the import cache.
void RegisterSoundLoader( AssetRegistry& registry );

// ---- the device ---------------------------------------------------------------

// The process's audio: the device and the mix - buses under the master,
// their volumes the player's settings - and the ears worlds are heard
// with. One a process, made by the app before its worlds and gone after.
// Offline it has no device, and what it mixes is read with ReadFrames:
// tests.
class AudioDevice
{
public:
    // The system's default output; why not, when there is none.
    static expected<Scope<AudioDevice>, string> TryStart();
    // Stereo, mixed only when read.
    static Scope<AudioDevice> Offline( u32 sampleRate = 48000 );
    ~AudioDevice();
    AudioDevice( const AudioDevice& ) = delete;
    AudioDevice& operator=( const AudioDevice& ) = delete;

    // music, sfx, ui and voice; master is the mix of them all.
    span<const Name> Buses() const;
    bool HasBus( Name bus ) const;
    // From 0, silent; master too. A bus there is not is a logic_error.
    void SetBusVolume( Name bus, f32 volume );
    f32 BusVolume( Name bus ) const;
    // "music, sfx, ui, voice", for messages.
    string BusNames() const;

    // Offline: the next `frames` frames of the mix, interleaved stereo.
    vector<f32> ReadFrames( u32 frames );
    // A browser starts audio only once the page saw a gesture: the app
    // calls this on input. Elsewhere the device runs already.
    void Resume();

    struct Impl;
    Impl& GetImpl() { return *mImpl; }

private:
    explicit AudioDevice( Scope<Impl> impl );
    Scope<Impl> mImpl;
};

// ---- components ---------------------------------------------------------------

// A sound an entity makes: through its bus, from where the entity is - or,
// not spatial, the same everywhere: music, a click.
struct AudioSource
{
    AssetId mSound;
    Name mBus = "sfx"_n;
    f32 mVolume = 1;
    f32 mPitch = 1;
    bool mLoop = false;
    // Plays as soon as it is in the world and its sound is in.
    bool mAutoplay = true;
    bool mSpatial = true;
    // Full volume nearer than the first, quieter as it goes, and no quieter
    // past the second.
    f32 mMinDistance = 1;
    f32 mMaxDistance = 50;
};

// Where a world is heard from: the first entity with an active one; with
// none, the first camera.
struct AudioListener
{
    bool mActive = true;
};

// What a world plays at once, at most: past it, the quietest gives way.
constexpr u32 cMaxVoices = 32;

// A world's audio as the rest of the engine asks it: a service.
class AudioWorld
{
public:
    virtual ~AudioWorld() = default;

    // A sound once, through `bus`; at `position` when one is given, else
    // the same everywhere. It plays when its sound is in - dropped if that
    // takes a second; dropped too when the world has no room for it.
    virtual void PlaySound( const AssetId& sound, f32 volume, Name bus, opt<vec3> position ) = 0;
    // An audio_source's sound, from the start; false for an entity with none.
    virtual bool TryPlay( Entity source ) = 0;
    virtual bool TryStop( Entity source ) = 0;
    virtual bool Playing( Entity source ) const = 0;
    virtual u32 Voices() const = 0;
    // Not audible: no voice plays - a prefab's preview. Audible by default.
    virtual void SetAudible( bool audible ) = 0;
    virtual AudioDevice& Device() = 0;
};

// audio_source and audio_listener, played through `device`, which outlives
// every world; voices go with a world's snapshot and start again from the
// scene after it is restored.
ModuleType AudioModule( AssetRegistry& assets, AudioDevice& device );
}
