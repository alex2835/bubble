// The audio module on miniaudio, heard through an offline device: what it
// mixes is read back and measured. Sources and one-shots, buses, distance,
// the voice limit, a paused world, scripts, streamed sounds and importing.
#include "bubble/audio/audio.hpp"
#include "bubble/import/import_cache.hpp"
#include "bubble/import/script_importer.hpp"
#include "bubble/import/sound_importers.hpp"
#include "bubble/scene/hierarchy.hpp"
#include "bubble/scripts/script_module.hpp"
#include "bubble/types/algorithm.hpp"
#include "bubble/world/transform.hpp"
#include "bubble/world/world.hpp"
#include "log_watch.hpp"
#include <cstring>
#include <doctest.h>

using namespace bubble;
using namespace bubble::test;

namespace
{
void PutLittle( string& out, u32 value, u32 bytes )
{
    for ( u32 byte = 0; byte < bytes; ++byte )
        out.push_back( static_cast<char>( ( value >> ( 8 * byte ) ) & 0xFF ) );
}

// A sine as a 16-bit mono .wav file.
string MakeWav( f32 seconds, f32 frequency = 440.0f, f32 amplitude = 0.5f, u32 rate = 48000 )
{
    const auto frames = static_cast<u32>( seconds * static_cast<f32>( rate ) );
    string samples;
    for ( u32 frame = 0; frame < frames; ++frame )
    {
        const f32 value =
            amplitude * sin( 2.0f * glm::pi<f32>() * frequency * static_cast<f32>( frame ) / static_cast<f32>( rate ) );
        PutLittle( samples, static_cast<u32>( static_cast<u16>( static_cast<i16>( value * 32767.0f ) ) ), 2 );
    }
    string wav = "RIFF";
    PutLittle( wav, 36 + static_cast<u32>( samples.size() ), 4 );
    wav += "WAVEfmt ";
    PutLittle( wav, 16, 4 );
    PutLittle( wav, 1, 2 );
    PutLittle( wav, 1, 2 );
    PutLittle( wav, rate, 4 );
    PutLittle( wav, rate * 2, 4 );
    PutLittle( wav, 2, 2 );
    PutLittle( wav, 16, 2 );
    wav += "data";
    PutLittle( wav, static_cast<u32>( samples.size() ), 4 );
    return wav + samples;
}

f32 Rms( const vector<f32>& samples )
{
    f64 sum = 0;
    for ( const f32 sample : samples )
        sum += static_cast<f64>( sample ) * sample;
    return samples.empty() ? 0.0f : static_cast<f32>( sqrt( sum / static_cast<f64>( samples.size() ) ) );
}

struct AudioFixture
{
    hmap<string, string> mFiles;
    AssetCatalog mCatalog;
    Scope<AudioDevice> mDevice = AudioDevice::Offline();
    Importer mImporter = ScriptImporter();
    App mApp{ AppOptions{ .mThreads = 1 } };
    AssetRegistry mAssets{ mCatalog, AssetRegistry::AtOnce( [this]( const AssetId& assetId, string_view kind ) {
                               const auto found = mFiles.find( ImportedPath( assetId, kind ).View() );
                               if ( found == mFiles.end() )
                                   return expected<string, string>( unexpected( "no such file"s ) );
                               return expected<string, string>( found->second );
                           } ),
                           mApp.GetJobs() };
    vector<string> mRecorded;

    AudioFixture()
    {
        RegisterScriptLoader( mAssets );
        RegisterSoundLoader( mAssets );
        mApp.AddModule( TransformModule() );
        mApp.AddModule(
            ScriptModule( mAssets, ScriptModuleOptions{ .mGlobals =
                                                            [this]( LuaState& lua, World& ) {
                                                                lua.Globals()["record"] = [this]( string text ) {
                                                                    mRecorded.push_back( std::move( text ) );
                                                                };
                                                            },
                                                        .mAliases = {},
                                                        .mTimeLimit = 1.0,
                                                        .mProgress = {},
                                                        .mSaves = {},
                                                        .mGameVersion = {} } ) );
        mApp.AddModule( AudioModule( mAssets, *mDevice ) );
    }

    AssetId AddSound( string_view path, const string& wav, bool stream = false )
    {
        const AssetPath assetPath = *AssetPath::TryFrom( path );
        if ( not mCatalog.TryIdOf( assetPath ) )
            mCatalog.Add( AssetId::Generate(), assetPath );
        const AssetId assetId = *mCatalog.TryIdOf( assetPath );
        mFiles[ImportedPath( assetId, cSoundKind ).String()] = WriteSoundAsset( wav, stream );
        return assetId;
    }

    AssetId AddScript( string_view path, string_view source )
    {
        mFiles[string( path )] = string( source );
        const AssetPath assetPath = *AssetPath::TryFrom( path );
        if ( not mCatalog.TryIdOf( assetPath ) )
            mCatalog.Add( AssetId::Generate(), assetPath );
        const AssetId assetId = *mCatalog.TryIdOf( assetPath );
        REQUIRE(
            TryImportAsset( ProjectFiles::InMemory( mFiles ), mImporter, assetPath, assetId, Json( Json::Object() ) ) );
        return assetId;
    }

    static Entity Place( Scene& scene, string_view name, vec3 position )
    {
        const Entity entity = CreateChild( scene, scene.Root(), *Text::TryFrom( name ) );
        scene.Add<Transform>( entity, Transform{ .mPosition = position } );
        return entity;
    }

    // A few frames - loads come in, voices start - then a tenth of a second
    // of the mix, measured.
    f32 Loudness( World& world )
    {
        for ( int frame = 0; frame < 4; ++frame )
        {
            mApp.GetJobs().RunMainQueue();
            world.Frame( 1.0 / 60.0 );
        }
        return Rms( mDevice->ReadFrames( 4800 ) );
    }
};
}

TEST_CASE( "An audio_source plays its sound once it is in, through its bus, and stops" )
{
    AudioFixture f;
    const AssetId tone = f.AddSound( "sounds/tone.wav", MakeWav( 2.0f ) );
    Scene scene;
    const Entity speaker = AudioFixture::Place( scene, "speaker", vec3( 0.0f ) );
    scene.Add<AudioSource>( speaker, AudioSource{ .mSound = tone, .mSpatial = false } );
    World world( f.mApp, std::move( scene ) );

    CHECK( f.Loudness( world ) > 0.2f );
    CHECK( world.Get<AudioWorld>().Playing( speaker ) );
    f.mDevice->SetBusVolume( "sfx"_n, 0.0f );
    CHECK( f.Loudness( world ) < 0.001f );
    f.mDevice->SetBusVolume( "sfx"_n, 1.0f );
    REQUIRE( world.Get<AudioWorld>().TryStop( speaker ) );
    CHECK( f.Loudness( world ) < 0.001f );
    CHECK_FALSE( world.Get<AudioWorld>().Playing( speaker ) );
}

TEST_CASE( "Far from the listener a spatial sound is quieter; one that is not spatial is not" )
{
    AudioFixture f;
    const AssetId tone = f.AddSound( "sounds/tone.wav", MakeWav( 2.0f ) );
    const auto loudnessAt = [&]( f32 distance, bool spatial ) {
        Scene scene;
        const Entity ears = AudioFixture::Place( scene, "ears", vec3( 0.0f ) );
        scene.Add<AudioListener>( ears );
        const Entity speaker = AudioFixture::Place( scene, "speaker", vec3( distance, 0.0f, 0.0f ) );
        scene.Add<AudioSource>( speaker, AudioSource{ .mSound = tone, .mSpatial = spatial } );
        World world( f.mApp, std::move( scene ) );
        return f.Loudness( world );
    };
    const f32 near = loudnessAt( 1.0f, true );
    const f32 far = loudnessAt( 30.0f, true );
    CHECK( near > 0.1f );
    CHECK( far < near / 5.0f );
    CHECK( loudnessAt( 30.0f, false ) > 0.2f );
}

TEST_CASE( "A spatial sound's distance takes it from whole to nothing as its rolloff says" )
{
    // Whole nearer than min_distance, nothing from max_distance on.
    for ( const Rolloff rolloff : { Rolloff::Natural, Rolloff::Squared, Rolloff::Linear } )
    {
        CHECK( DistanceGain( rolloff, 1.0f, 2.0f, 10.0f ) == 1.0f );
        CHECK( DistanceGain( rolloff, 2.0f, 2.0f, 10.0f ) == 1.0f );
        CHECK( DistanceGain( rolloff, 10.0f, 2.0f, 10.0f ) == 0.0f );
        CHECK( DistanceGain( rolloff, 500.0f, 2.0f, 10.0f ) == 0.0f );
        // Quieter all the way out.
        f32 before = 1.0f;
        for ( f32 distance = 2.5f; distance < 10.0f; distance += 0.5f )
        {
            const f32 gain = DistanceGain( rolloff, distance, 2.0f, 10.0f );
            CHECK( gain < before );
            before = gain;
        }
    }
    // Half way out: natural the quietest, linear the loudest.
    CHECK( DistanceGain( Rolloff::Natural, 6.0f, 2.0f, 10.0f ) == doctest::Approx( ( 2.0f / 6 - 0.2f ) / 0.8f ) );
    CHECK( DistanceGain( Rolloff::Squared, 6.0f, 2.0f, 10.0f ) == doctest::Approx( 0.25f ) );
    CHECK( DistanceGain( Rolloff::Linear, 6.0f, 2.0f, 10.0f ) == doctest::Approx( 0.5f ) );
    // A min_distance of 0 is not silence everywhere.
    CHECK( DistanceGain( Rolloff::Natural, 0.5f, 0.0f, 10.0f ) > 0.4f );

    CHECK( TryRolloff( "squared"_n ) == Rolloff::Squared );
    const auto loudMaybe = TryRolloff( Name::Declare( "loud" ) );
    REQUIRE_FALSE( loudMaybe );
    CHECK( loudMaybe.error() == "audio_source.rolloff: loud is not one of natural, squared, linear" );
}

TEST_CASE( "Past its max_distance a spatial sound is silent; nearer, as quiet as its rolloff says" )
{
    AudioFixture f;
    const AssetId tone = f.AddSound( "sounds/tone.wav", MakeWav( 2.0f ) );
    const auto loudnessAt = [&]( f32 distance, Name rolloff ) {
        Scene scene;
        const Entity ears = AudioFixture::Place( scene, "ears", vec3( 0.0f ) );
        scene.Add<AudioListener>( ears );
        const Entity speaker = AudioFixture::Place( scene, "speaker", vec3( 0.0f, 0.0f, -distance ) );
        scene.Add<AudioSource>(
            speaker, AudioSource{ .mSound = tone, .mMinDistance = 1, .mMaxDistance = 5, .mRolloff = rolloff } );
        World world( f.mApp, std::move( scene ) );
        return f.Loudness( world );
    };
    const f32 near = loudnessAt( 1.0f, "natural"_n );
    CHECK( near > 0.1f );
    CHECK( loudnessAt( 6.0f, "natural"_n ) < 0.001f );
    CHECK( loudnessAt( 6.0f, "linear"_n ) < 0.001f );
    // Half way out, linear is loudest, natural quietest.
    const f32 natural = loudnessAt( 3.0f, "natural"_n );
    const f32 squared = loudnessAt( 3.0f, "squared"_n );
    const f32 linear = loudnessAt( 3.0f, "linear"_n );
    CHECK( natural > 0.01f );
    CHECK( natural < squared );
    CHECK( squared < linear );
    CHECK( linear == doctest::Approx( near * 0.5f ).epsilon( 0.05 ) );

    // A rolloff that is not one is said, and the natural one plays.
    const LogWatch log;
    CHECK( loudnessAt( 3.0f, Name::Declare( "loud" ) ) == doctest::Approx( natural ).epsilon( 0.05 ) );
    CHECK( log.Saw( LogLevel::Error,
                    { "speaker", "audio_source.rolloff: loud is not one of natural, squared, linear" } ) );

    // Still turned to its side: to the right, the right ear hears more.
    Scene scene;
    scene.Add<AudioListener>( AudioFixture::Place( scene, "ears", vec3( 0.0f ) ) );
    const Entity speaker = AudioFixture::Place( scene, "speaker", vec3( 2.0f, 0.0f, 0.0f ) );
    scene.Add<AudioSource>( speaker, AudioSource{ .mSound = tone, .mMinDistance = 1, .mMaxDistance = 5 } );
    World world( f.mApp, std::move( scene ) );
    (void)f.Loudness( world );
    const vector<f32> frames = f.mDevice->ReadFrames( 4800 );
    vector<f32> left;
    vector<f32> right;
    for ( size_t at = 0; at + 1 < frames.size(); at += 2 )
    {
        left.push_back( frames[at] );
        right.push_back( frames[at + 1] );
    }
    CHECK( Rms( right ) > Rms( left ) * 2.0f );
}

TEST_CASE( "A world plays thirty-two voices at most; past them, a sound waits its turn or the quietest gives way" )
{
    AudioFixture f;
    const AssetId tone = f.AddSound( "sounds/tone.wav", MakeWav( 2.0f, 440.0f, 0.1f ) );
    World world( f.mApp, Scene() );
    AudioWorld& audio = world.Get<AudioWorld>();
    for ( int shot = 0; shot < 40; ++shot )
        audio.PlaySound( tone, 0.2f, "sfx"_n, nullopt );
    (void)f.Loudness( world );
    CHECK( audio.Voices() == cMaxVoices );
    // A louder one takes a quiet one's place; the count stays.
    audio.PlaySound( tone, 1.0f, "sfx"_n, nullopt );
    (void)f.Loudness( world );
    CHECK( audio.Voices() == cMaxVoices );
}

TEST_CASE( "A paused world is silent but for its UI, and goes on where it stopped" )
{
    AudioFixture f;
    const AssetId tone = f.AddSound( "sounds/tone.wav", MakeWav( 3.0f ) );
    Scene scene;
    const Entity music = AudioFixture::Place( scene, "music", vec3( 0.0f ) );
    scene.Add<AudioSource>( music, AudioSource{ .mSound = tone, .mBus = "music"_n, .mSpatial = false } );
    World world( f.mApp, std::move( scene ) );
    CHECK( f.Loudness( world ) > 0.2f );

    world.SetPaused( true );
    CHECK( f.Loudness( world ) < 0.001f );
    world.Get<AudioWorld>().PlaySound( tone, 1.0f, "ui"_n, nullopt );
    CHECK( f.Loudness( world ) > 0.2f );
    // Going on at its next frame.
    world.SetPaused( false );
    CHECK( f.Loudness( world ) > 0.2f );
    CHECK( world.Get<AudioWorld>().Playing( music ) );
}

TEST_CASE( "Scripts play sounds once and start and stop their sources; a bus that is not there is said" )
{
    AudioFixture f;
    const AssetId tone = f.AddSound( "sounds/tone.wav", MakeWav( 2.0f ) );
    const AssetId script = f.AddScript( "scripts/speaker.luau", R"(
local P = props { beep = asset( "sounds/tone.wav" ) }
function on_start( self )
    audio.play_sound( self.beep, 0.5, "ui" )
    audio.set_bus_volume( "music", 0.25 )
    record( string.format( "%g", audio.bus_volume( "music" ) ) )
end
function on_update( self, dt )
    local source = self.entity.audio_source
    if not self.started then
        self.started = true
        source:play()
    elseif source.playing and not self.heard then
        self.heard = true
        record( "playing" )
        source:stop()
    end
end
locals { started = false, heard = false }
)" );
    (void)tone;
    Scene scene;
    const Entity speaker = AudioFixture::Place( scene, "speaker", vec3( 0.0f ) );
    scene.Add<AudioSource>( speaker, AudioSource{ .mSound = tone, .mAutoplay = false, .mSpatial = false } );
    scene.Add<Script>( speaker, Script{ .mAsset = script, .mProps = Value( Value::Record{} ) } );
    World world( f.mApp, std::move( scene ) );
    CHECK( f.Loudness( world ) > 0.1f );
    for ( int frame = 0; frame < 6; ++frame )
        world.Frame( 1.0 / 60.0 );
    CHECK( f.mRecorded == vector<string>{ "0.25", "playing" } );
    CHECK_FALSE( world.Get<AudioWorld>().Playing( speaker ) );
    f.mDevice->SetBusVolume( "music"_n, 1.0f );

    const LogWatch log;
    const AssetId wrong =
        f.AddScript( "scripts/wrong.luau", "function on_start( self ) audio.set_bus_volume( 'musik', 1 ) end" );
    Scene other;
    const Entity entity = AudioFixture::Place( other, "wrong", vec3( 0.0f ) );
    other.Add<Script>( entity, Script{ .mAsset = wrong, .mProps = Value( Value::Record{} ) } );
    World second( f.mApp, std::move( other ) );
    (void)f.Loudness( second );
    CHECK( log.Saw( LogLevel::Error, { "no audio bus \"musik\" (buses: master, music, sfx, ui, voice)" } ) );
}

TEST_CASE( "A streamed sound plays as it is decoded" )
{
    AudioFixture f;
    const AssetId music = f.AddSound( "sounds/music.wav", MakeWav( 3.0f ), true );
    World world( f.mApp, Scene() );
    world.Get<AudioWorld>().PlaySound( music, 1.0f, "music"_n, nullopt );
    CHECK( f.Loudness( world ) > 0.2f );
    const auto assetRef = f.mAssets.LoadNow<SoundAsset>( music );
    REQUIRE( assetRef.Ready() );
    CHECK_FALSE( assetRef.TryGet()->mData->mEncoded.empty() );
    CHECK( assetRef.TryGet()->mData->mFrames.empty() );
}

TEST_CASE( "Sounds are imported as they are, longer ones to stream; what does not decode is refused" )
{
    const vector<Importer> importers = SoundImporters();
    REQUIRE( importers.size() == 3 );
    const Importer& wav = *ranges::find( importers, ".wav"s, &Importer::mExtension );
    const AssetPath path = *AssetPath::TryFrom( "sounds/a.wav" );
    const auto shortMaybe = wav.mImport( MakeWav( 0.5f ), Json(), path );
    REQUIRE_MESSAGE( shortMaybe, shortMaybe.error() );
    CHECK( shortMaybe->mBytes.substr( 8, 4 ) == string( "\0\0\0\0", 4 ) );
    Json::Object settings;
    settings.push_back( Json::Member{ "stream", Json( true ) } );
    const auto streamedMaybe = wav.mImport( MakeWav( 0.5f ), Json( settings ), path );
    REQUIRE( streamedMaybe );
    CHECK( streamedMaybe->mBytes.substr( 8, 4 ) == string( "\1\0\0\0", 4 ) );
    CHECK( wav.mImport( "not a sound", Json(), path ).error() ==
           "sounds/a.wav: not a sound miniaudio decodes - wav, flac or mp3" );
    const auto infoMaybe = TryProbeSound( MakeWav( 1.5f ) );
    REQUIRE( infoMaybe );
    CHECK( infoMaybe->mSeconds == doctest::Approx( 1.5 ) );
    CHECK( infoMaybe->mSampleRate == 48000 );
}
