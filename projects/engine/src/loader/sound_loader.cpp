#include "engine/pch/pch.hpp"
#include "engine/loader/loader.hpp"
#include <miniaudio.h>

namespace bubble
{
// Streaming is a property of how a sound is used, not of the file, but the
// distinction has to be decided somewhere and the asset is the only place that
// survives serialisation. Long files are music and get streamed; short ones are
// effects and get decoded up front. Overridable per asset afterwards.
constexpr f32 STREAMING_THRESHOLD_SECONDS = 15.0f;

Ref<Sound> LoadSound( const path& soundPath )
{
    if ( not filesystem::exists( soundPath ) )
        return nullptr;

    // Decoding the header here is the whole point of loading a sound eagerly:
    // it is the only moment where a bad path or an unsupported codec can be
    // reported against the file that caused it. Playback happens on the audio
    // thread, where the same failure is a silent no-op.
    ma_decoder decoder;
    const string filePath = soundPath.string();
    if ( ma_decoder_init_file( filePath.c_str(), nullptr, &decoder ) != MA_SUCCESS )
    {
        LogError( "Failed to decode sound: {}", filePath );
        return nullptr;
    }

    ma_uint64 frames = 0;
    ma_decoder_get_length_in_pcm_frames( &decoder, &frames );
    const f32 seconds = decoder.outputSampleRate > 0
                            ? (f32)frames / (f32)decoder.outputSampleRate
                            : 0.0f;
    ma_decoder_uninit( &decoder );

    return CreateRef<Sound>(
        Sound{
            .mName = soundPath.stem().string(),
            .mPath = soundPath,
            .mStreaming = seconds > STREAMING_THRESHOLD_SECONDS
        }
    );
}


Ref<Sound> Loader::LoadSound( const path& soundPath )
{
    auto [relPath, absPath] = RelAbsFromProjectPath( soundPath );

    auto iter = mSounds.find( relPath );
    if ( iter != mSounds.end() )
        return iter->second;

    auto sound = bubble::LoadSound( absPath );
    if ( not sound )
    {
        LogError( "Failed to load sound: {}", absPath.string() );
        return nullptr;
    }

    mSounds.emplace( relPath, sound );
    mResourcesGeneration = NextResourcesGeneration();
    return sound;
}

}
