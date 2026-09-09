#include "engine/pch/pch.hpp"
#include "engine/audio/audio_engine.hpp"
#include "engine/log/log.hpp"
#include <miniaudio.h>
#include <format>
#include <stdexcept>
#include <vector>

#if defined( __EMSCRIPTEN__ )
#include <emscripten/emscripten.h>
#endif

namespace bubble
{
AudioEngine* AudioEngine::sInstance = nullptr;

AudioEngine::AudioEngine()
    : mEngine( CreateScope<ma_engine>() )
{
    ma_engine_config config = ma_engine_config_init();

    const ma_result result = ma_engine_init( &config, mEngine.get() );
    if ( result != MA_SUCCESS )
    {
        // Not fatal. A machine with no audio device at all is a normal thing to
        // develop on, and an engine that refuses to start because of it would
        // be worse than one that runs silently. Every entry point below checks
        // mInitialized, so nothing has to test for a null engine.
        LogError( "AudioEngine: failed to initialize audio device ({}). Continuing without sound.",
                  (int)result );
        mEngine.reset();
    }
    else
    {
        mInitialized = true;
    }

    sInstance = this;
}

AudioEngine::~AudioEngine()
{
    // Voices first: every ma_sound is a node in the engine's graph, and
    // uninitializing the graph out from under one is a use after free.
    StopAll();

    if ( mInitialized )
        ma_engine_uninit( mEngine.get() );

    if ( sInstance == this )
        sInstance = nullptr;
}

AudioEngine& AudioEngine::Get()
{
    if ( not sInstance )
        throw std::runtime_error( "AudioEngine::Get() called with no AudioEngine alive" );
    return *sInstance;
}

bool AudioEngine::Exists()
{
    return sInstance != nullptr;
}


AudioEngine::Voice* AudioEngine::FindVoice( u64 id )
{
    auto iter = mVoices.find( id );
    return iter != mVoices.end() ? &iter->second : nullptr;
}

const AudioEngine::Voice* AudioEngine::FindVoice( u64 id ) const
{
    auto iter = mVoices.find( id );
    return iter != mVoices.end() ? &iter->second : nullptr;
}


VoiceHandle AudioEngine::Play( const Ref<Sound>& sound, const VoiceParams& params )
{
    if ( not mInitialized or not sound )
        return VoiceHandle();

    Voice voice{ CreateScope<ma_sound>(), sound };

    // MA_SOUND_FLAG_DECODE reads and decodes the whole file up front, which is
    // what a sound effect wants - the alternative is a file read on the audio
    // thread the first time it is triggered. Music takes the streaming path
    // instead, where holding minutes of decoded PCM is the worse trade.
    const ma_uint32 flags = sound->mStreaming ? MA_SOUND_FLAG_STREAM : MA_SOUND_FLAG_DECODE;

    // Deliberately not MA_SOUND_FLAG_NO_SPATIALIZATION for a 2D voice: that
    // flag skips building the spatializer entirely, and a voice without one
    // can never be turned back into a 3D voice. Since scripts flip
    // `spatialized` at runtime, every voice gets a spatializer and 2D is
    // expressed by disabling it below. The cost is a few unused coefficients.

    const string filePath = sound->mPath.string();
    const ma_result result = ma_sound_init_from_file( mEngine.get(), filePath.c_str(),
                                                      flags, nullptr, nullptr, voice.mSound.get() );
    if ( result != MA_SUCCESS )
    {
        LogError( "AudioEngine: failed to start '{}' ({})", filePath, (int)result );
        return VoiceHandle();
    }

    const u64 id = mNextVoiceId++;
    auto [iter, inserted] = mVoices.emplace( id, std::move( voice ) );

    ApplyVoiceParams( VoiceHandle( id ), params );

    if ( ma_sound_start( iter->second.mSound.get() ) != MA_SUCCESS )
    {
        LogError( "AudioEngine: failed to start playback of '{}'", filePath );
        ma_sound_uninit( iter->second.mSound.get() );
        mVoices.erase( iter );
        return VoiceHandle();
    }

    return VoiceHandle( id );
}

void AudioEngine::Stop( VoiceHandle& voice )
{
    if ( Voice* found = FindVoice( voice.Id() ) )
    {
        ma_sound_uninit( found->mSound.get() );
        mVoices.erase( voice.Id() );
    }
    // Cleared even when the voice was already gone - a handle to a finished
    // voice is exactly as stale as one to a stopped voice.
    voice.Clear();
}

void AudioEngine::StopAll()
{
    for ( auto& [id, voice] : mVoices )
        ma_sound_uninit( voice.mSound.get() );
    mVoices.clear();
}

bool AudioEngine::IsPlaying( const VoiceHandle& voice ) const
{
    const Voice* found = FindVoice( voice.Id() );
    return found and ma_sound_is_playing( found->mSound.get() );
}

void AudioEngine::SetVoicePosition( const VoiceHandle& voice, const vec3& position )
{
    if ( Voice* found = FindVoice( voice.Id() ) )
        ma_sound_set_position( found->mSound.get(), position.x, position.y, position.z );
}

void AudioEngine::ApplyVoiceParams( const VoiceHandle& voice, const VoiceParams& params )
{
    Voice* found = FindVoice( voice.Id() );
    if ( not found )
        return;

    ma_sound* sound = found->mSound.get();
    ma_sound_set_volume( sound, params.mVolume );
    ma_sound_set_pitch( sound, params.mPitch );
    ma_sound_set_looping( sound, params.mLooping ? MA_TRUE : MA_FALSE );

    // Spatialization is also a creation flag, because a sound initialized
    // without it has no spatializer to turn back on. Setting it here keeps a
    // voice that *was* created spatialized switchable, and is harmless on one
    // that was not.
    ma_sound_set_spatialization_enabled( sound, params.mSpatialized ? MA_TRUE : MA_FALSE );
    if ( params.mSpatialized )
    {
        ma_sound_set_position( sound, params.mPosition.x, params.mPosition.y, params.mPosition.z );
        ma_sound_set_min_distance( sound, params.mMinDistance );
        ma_sound_set_max_distance( sound, params.mMaxDistance );
        ma_sound_set_rolloff( sound, params.mRolloff );
    }
}

void AudioEngine::SetListener( const vec3& position, const vec3& forward, const vec3& up )
{
    if ( not mInitialized )
        return;

    constexpr ma_uint32 listener = 0;
    ma_engine_listener_set_position( mEngine.get(), listener, position.x, position.y, position.z );
    ma_engine_listener_set_direction( mEngine.get(), listener, forward.x, forward.y, forward.z );
    ma_engine_listener_set_world_up( mEngine.get(), listener, up.x, up.y, up.z );
}

f32 AudioEngine::GetMasterVolume() const
{
    if ( not mInitialized )
        return 0.0f;
    return ma_engine_get_volume( mEngine.get() );
}

void AudioEngine::SetMasterVolume( f32 volume )
{
    if ( mInitialized )
        ma_engine_set_volume( mEngine.get(), volume );
}

void AudioEngine::OnUpdate()
{
    TryResumeAudioContext();

    if ( not mInitialized or mVoices.empty() )
        return;

    // Collected first: erasing from the map while walking it would invalidate
    // the iterator, and the finished set is almost always empty or tiny.
    std::vector<u64> finished;
    for ( auto& [id, voice] : mVoices )
    {
        if ( ma_sound_at_end( voice.mSound.get() ) )
            finished.push_back( id );
    }

    for ( const u64 id : finished )
    {
        if ( Voice* voice = FindVoice( id ) )
        {
            ma_sound_uninit( voice->mSound.get() );
            mVoices.erase( id );
        }
    }
}

void AudioEngine::TryResumeAudioContext()
{
#if defined( __EMSCRIPTEN__ )
    if ( not mInitialized or mAudioContextRunning )
        return;

    // miniaudio owns the AudioContext and exposes no C entry point that resumes
    // it, so this reaches the one its Web Audio backend created. The global is
    // absent entirely until a device has been started, hence the guard rather
    // than a bare resume().
    mAudioContextRunning = EM_ASM_INT( {
        if ( typeof window === 'undefined' || !window.miniaudio || !window.miniaudio.context )
            return 0;
        if ( window.miniaudio.context.state === 'running' )
            return 1;
        window.miniaudio.context.resume();
        return 0;
    } ) != 0;
#endif
}

}
