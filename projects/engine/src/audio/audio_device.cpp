#include "audio_device_impl.hpp"
#include "bubble/assets/asset_registry.hpp"
#include "bubble/data/binary.hpp"
#include "bubble/types/algorithm.hpp"
#include "bubble/types/format.hpp"

namespace bubble
{
namespace
{
constexpr string_view cMagic = "BSND";
constexpr u32 cFormat = 1;

// A decoder over `bytes`, f32 in the file's channels and rate.
expected<Scope<ma_decoder>, string> TryDecoder( string_view bytes )
{
    auto decoder = CreateScope<ma_decoder>();
    const ma_decoder_config config = ma_decoder_config_init( ma_format_f32, 0, 0 );
    if ( ma_decoder_init_memory( bytes.data(), bytes.size(), &config, decoder.get() ) != MA_SUCCESS )
        return unexpected( "not a sound miniaudio decodes - wav, flac or mp3"s );
    return decoder;
}

expected<SoundData, string> TryDecode( string_view bytes, bool stream )
{
    auto decoderMaybe = TryDecoder( bytes );
    if ( not decoderMaybe )
        return unexpected( std::move( decoderMaybe.error() ) );
    ma_decoder& decoder = **decoderMaybe;
    SoundData data;
    data.mChannels = decoder.outputChannels;
    data.mSampleRate = decoder.outputSampleRate;
    if ( stream )
    {
        ma_uint64 length = 0;
        ma_decoder_get_length_in_pcm_frames( &decoder, &length );
        data.mSeconds = static_cast<f64>( length ) / data.mSampleRate;
        data.mEncoded = string( bytes );
    }
    else
    {
        // In pieces: an mp3's length is a guess until it is read.
        constexpr ma_uint64 cPiece = 4096;
        for ( ;; )
        {
            const size_t at = data.mFrames.size();
            data.mFrames.resize( at + cPiece * data.mChannels );
            ma_uint64 read = 0;
            ma_decoder_read_pcm_frames( &decoder, data.mFrames.data() + at, cPiece, &read );
            data.mFrames.resize( at + read * data.mChannels );
            if ( read < cPiece )
                break;
        }
        data.mSeconds = static_cast<f64>( data.mFrames.size() / data.mChannels ) / data.mSampleRate;
    }
    ma_decoder_uninit( &decoder );
    return data;
}
}

AudioDevice::Impl::~Impl()
{
    for ( const Scope<ma_sound_group>& group : mGroups )
        ma_sound_group_uninit( group.get() );
    ma_engine_uninit( &mEngine );
}

ma_sound_group* AudioDevice::Impl::TryGroup( Name bus )
{
    const auto found = ranges::find( mBuses, bus );
    return found == mBuses.end() ? nullptr : mGroups[static_cast<size_t>( found - mBuses.begin() )].get();
}

AudioDevice::AudioDevice( Scope<Impl> impl ) : mImpl( std::move( impl ) )
{
    for ( const string_view bus : { "music", "sfx", "ui", "voice" } )
    {
        mImpl->mBuses.push_back( Name::Declare( bus ) );
        auto group = CreateScope<ma_sound_group>();
        if ( ma_sound_group_init( &mImpl->mEngine, 0, nullptr, group.get() ) != MA_SUCCESS )
            throw logic_error( format( "the audio bus {} cannot be made", bus ) );
        mImpl->mGroups.push_back( std::move( group ) );
    }
    (void)Name::Declare( "master" );
}

AudioDevice::~AudioDevice() = default;

expected<Scope<AudioDevice>, string> AudioDevice::TryStart()
{
    auto impl = CreateScope<Impl>();
    const ma_engine_config config = ma_engine_config_init();
    if ( const ma_result result = ma_engine_init( &config, &impl->mEngine ); result != MA_SUCCESS )
        return unexpected( format( "no audio device: {}", ma_result_description( result ) ) );
    impl->mStarted = true;
    return Scope<AudioDevice>( new AudioDevice( std::move( impl ) ) );
}

Scope<AudioDevice> AudioDevice::Offline( u32 sampleRate )
{
    auto impl = CreateScope<Impl>();
    ma_engine_config config = ma_engine_config_init();
    config.noDevice = MA_TRUE;
    config.channels = 2;
    config.sampleRate = sampleRate;
    if ( ma_engine_init( &config, &impl->mEngine ) != MA_SUCCESS )
        throw logic_error( "an offline audio engine cannot be made" );
    impl->mOffline = true;
    impl->mStarted = true;
    return Scope<AudioDevice>( new AudioDevice( std::move( impl ) ) );
}

span<const Name> AudioDevice::Buses() const
{
    return mImpl->mBuses;
}

bool AudioDevice::HasBus( Name bus ) const
{
    return bus == "master"_n or ranges::find( mImpl->mBuses, bus ) != mImpl->mBuses.end();
}

void AudioDevice::SetBusVolume( Name bus, f32 volume )
{
    if ( bus == "master"_n )
        ma_engine_set_volume( &mImpl->mEngine, volume );
    else if ( ma_sound_group* group = mImpl->TryGroup( bus ) )
        ma_sound_group_set_volume( group, volume );
    else
        throw logic_error( format( "no audio bus {} (buses: master, {})", bus.ToString(), BusNames() ) );
}

f32 AudioDevice::BusVolume( Name bus ) const
{
    if ( bus == "master"_n )
        return ma_engine_get_volume( &mImpl->mEngine );
    if ( ma_sound_group* group = mImpl->TryGroup( bus ) )
        return ma_sound_group_get_volume( group );
    throw logic_error( format( "no audio bus {} (buses: master, {})", bus.ToString(), BusNames() ) );
}

string AudioDevice::BusNames() const
{
    string names;
    for ( const Name bus : mImpl->mBuses )
        names += ( names.empty() ? "" : ", " ) + bus.ToString();
    return names;
}

vector<f32> AudioDevice::ReadFrames( u32 frames )
{
    if ( not mImpl->mOffline )
        throw logic_error( "AudioDevice::ReadFrames of a device that plays: the device reads the mix itself" );
    vector<f32> mix( static_cast<size_t>( frames ) * 2, 0.0f );
    ma_uint64 read = 0;
    ma_engine_read_pcm_frames( &mImpl->mEngine, mix.data(), frames, &read );
    mix.resize( static_cast<size_t>( read ) * 2 );
    return mix;
}

void AudioDevice::Resume()
{
    if ( not mImpl->mOffline )
        ma_engine_start( &mImpl->mEngine );
}

expected<SoundInfo, string> TryProbeSound( string_view bytes )
{
    auto decoderMaybe = TryDecoder( bytes );
    if ( not decoderMaybe )
        return unexpected( std::move( decoderMaybe.error() ) );
    ma_decoder& decoder = **decoderMaybe;
    ma_uint64 length = 0;
    ma_decoder_get_length_in_pcm_frames( &decoder, &length );
    const SoundInfo info{ .mChannels = decoder.outputChannels,
                          .mSampleRate = decoder.outputSampleRate,
                          .mSeconds = static_cast<f64>( length ) / decoder.outputSampleRate };
    ma_decoder_uninit( &decoder );
    if ( info.mChannels == 0 or info.mSampleRate == 0 )
        return unexpected( "a sound with no channels or no rate"s );
    return info;
}

string WriteSoundAsset( string_view file, bool stream )
{
    BinaryWriter out;
    out.Bytes( cMagic );
    out.U32( cFormat );
    out.U32( stream ? 1 : 0 );
    out.Bytes( file );
    return out.Take();
}

void RegisterSoundLoader( AssetRegistry& registry )
{
    registry.RegisterLoader<SoundAsset>(
        cSoundKind, []( string_view bytes, const AssetPath& ) -> expected<SoundAsset, string> {
            BinaryReader in( bytes );
            if ( in.TryBytes( cMagic.size() ) != cMagic )
                return unexpected( "not a sound the importer wrote"s );
            if ( const auto formatMaybe = in.TryU32(); formatMaybe != cFormat )
                return unexpected(
                    format( "a sound of format {}, this build reads {}", formatMaybe.value_or( 0 ), cFormat ) );
            const auto streamMaybe = in.TryU32();
            if ( not streamMaybe )
                return unexpected( "the imported sound is damaged"s );
            auto dataMaybe = TryDecode( bytes.substr( cMagic.size() + 8 ), *streamMaybe != 0 );
            if ( not dataMaybe )
                return unexpected( std::move( dataMaybe.error() ) );
            return SoundAsset{ CreateRef<const SoundData>( std::move( *dataMaybe ) ) };
        } );
}
}
