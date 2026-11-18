#include "audio_device_impl.hpp"
#include "bubble/assets/asset_registry.hpp"
#include "bubble/core/log.hpp"
#include "bubble/core/profile.hpp"
#include "bubble/scene/hierarchy.hpp"
#include "bubble/scripts/script_api.hpp"
#include "bubble/types/algorithm.hpp"
#include "bubble/types/format.hpp"
#include "bubble/world/transform.hpp"
#include "bubble/world/world.hpp"

namespace bubble
{
namespace
{
// One sound playing: its data held while it plays, the source miniaudio
// reads it through, and the sound in the mix. Never moved: miniaudio's
// objects point into themselves.
class Voice
{
public:
    Voice() = default;
    Voice( const Voice& ) = delete;
    Voice& operator=( const Voice& ) = delete;

    ~Voice()
    {
        // The sound reads the source, the source the data: in that order.
        if ( mSound )
            ma_sound_uninit( mSound.get() );
        if ( mBuffer )
            ma_audio_buffer_uninit( mBuffer.get() );
        if ( mDecoder )
            ma_decoder_uninit( mDecoder.get() );
    }

    // False when miniaudio would not have it.
    bool TryStart( ma_engine& engine, ma_sound_group& group, Ref<const SoundData> data, bool spatial )
    {
        mData = std::move( data );
        ma_data_source* source = nullptr;
        if ( mData->mEncoded.empty() )
        {
            mBuffer = CreateScope<ma_audio_buffer>();
            const ma_audio_buffer_config config =
                ma_audio_buffer_config_init( ma_format_f32, mData->mChannels, mData->mFrames.size() / mData->mChannels,
                                             mData->mFrames.data(), nullptr );
            if ( ma_audio_buffer_init( &config, mBuffer.get() ) != MA_SUCCESS )
            {
                mBuffer.reset();
                return false;
            }
            source = mBuffer.get();
        }
        else
        {
            mDecoder = CreateScope<ma_decoder>();
            const ma_decoder_config config = ma_decoder_config_init( ma_format_f32, 0, 0 );
            if ( ma_decoder_init_memory( mData->mEncoded.data(), mData->mEncoded.size(), &config, mDecoder.get() ) !=
                 MA_SUCCESS )
            {
                mDecoder.reset();
                return false;
            }
            source = mDecoder.get();
        }
        mSound = CreateScope<ma_sound>();
        const ma_uint32 flags = spatial ? 0 : MA_SOUND_FLAG_NO_SPATIALIZATION;
        if ( ma_sound_init_from_data_source( &engine, source, flags, &group, mSound.get() ) != MA_SUCCESS )
        {
            mSound.reset();
            return false;
        }
        return true;
    }

    ma_sound& Sound() { return *mSound; }
    const ma_sound& Sound() const { return *mSound; }

    // An audio_source's, or none: a sound played once.
    Entity mSource;
    Name mBus;
    f32 mVolume = 1;
    bool mSpatial = false;
    f32 mMinDistance = 1;
    f32 mMaxDistance = 50;
    // Where a one-shot plays; a source's follows its entity.
    vec3 mPosition{ 0.0f };
    // Stopped by a paused world, to go on with it.
    bool mHeld = false;

private:
    Ref<const SoundData> mData;
    Scope<ma_audio_buffer> mBuffer;
    Scope<ma_decoder> mDecoder;
    Scope<ma_sound> mSound;
};

// A sound to play once its asset is in.
struct Waiting
{
    AssetRef<SoundAsset> mAssetRef;
    Entity mSource;
    f32 mVolume = 1;
    Name mBus;
    opt<vec3> mPosition;
    f32 mAge = 0;
};

constexpr f32 cLongestWait = 1.0f;

class Audio final : public Module, public AudioWorld
{
public:
    Audio( World& world, AssetRegistry& assets, AudioDevice& device )
        : mWorld( world ),
          mAssets( assets ),
          mDevice( device )
    {
        const auto restart = [this]( Entity entity ) { Restart( entity ); };
        world.OnAdded<AudioSource>( restart );
        world.OnChanged<AudioSource>( "sound"_n, restart );
        world.OnChanged<AudioSource>( "bus"_n, restart );
        world.OnChanged<AudioSource>( "spatial"_n, restart );
        const auto apply = [this]( Entity entity ) { Apply( entity ); };
        for ( const Name field : { "volume"_n, "pitch"_n, "loop"_n, "min_distance"_n, "max_distance"_n } )
            world.OnChanged<AudioSource>( field, apply );
        world.OnRemoved<AudioSource>( [this]( Entity entity ) { Stop( entity ); } );
        world.AddStep( Phase::Audio, [this]( f32 dt ) { Update( dt ); } );
        world.GetServices().Add<AudioWorld>( *this );
    }

    ~Audio() override = default;

    void Restoring( OptRef<const ModuleSnapshot> ) override
    {
        // Sound does not play back: what played goes, and the scene that
        // comes back starts its sources again.
        mVoices.clear();
        mWaiting.clear();
    }

    // ---- AudioWorld ----------------------------------------------------------------

    void PlaySound( const AssetId& sound, f32 volume, Name bus, opt<vec3> position ) override
    {
        if ( not mAudible or not sound )
            return;
        mWaiting.push_back( Waiting{ .mAssetRef = mAssets.Load<SoundAsset>( sound ),
                                     .mSource = Entity::Null,
                                     .mVolume = volume,
                                     .mBus = bus,
                                     .mPosition = position,
                                     .mAge = 0 } );
        StartWaiting();
    }

    bool TryPlay( Entity source ) override
    {
        const auto sourceMaybe = mWorld.GetScene().TryGet<AudioSource>( source );
        if ( not sourceMaybe )
            return false;
        Stop( source );
        if ( mAudible and sourceMaybe->mSound )
        {
            mWaiting.push_back( Waiting{ .mAssetRef = mAssets.Load<SoundAsset>( sourceMaybe->mSound ),
                                         .mSource = source,
                                         .mVolume = sourceMaybe->mVolume,
                                         .mBus = sourceMaybe->mBus,
                                         .mPosition = nullopt,
                                         .mAge = 0 } );
            StartWaiting();
        }
        return true;
    }

    bool TryStop( Entity source ) override
    {
        if ( not mWorld.GetScene().Has<AudioSource>( source ) )
            return false;
        Stop( source );
        return true;
    }

    bool Playing( Entity source ) const override
    {
        for ( const Scope<Voice>& voice : mVoices )
            if ( voice->mSource == source and ma_sound_is_playing( &voice->Sound() ) )
                return true;
        return false;
    }

    u32 Voices() const override { return static_cast<u32>( mVoices.size() ); }

    void SetAudible( bool audible ) override
    {
        mAudible = audible;
        if ( not audible )
        {
            mVoices.clear();
            mWaiting.clear();
        }
    }

    AudioDevice& Device() override { return mDevice; }

private:
    // The entity's source as its component says now: what played stops, and
    // it plays from the start if it plays by itself.
    void Restart( Entity entity )
    {
        Stop( entity );
        const auto sourceMaybe = mWorld.GetScene().TryGet<AudioSource>( entity );
        if ( sourceMaybe and sourceMaybe->mAutoplay )
            (void)TryPlay( entity );
    }

    void Stop( Entity entity )
    {
        erase_if( mVoices, [&]( const Scope<Voice>& voice ) { return voice->mSource == entity; } );
        erase_if( mWaiting, [&]( const Waiting& waiting ) { return waiting.mSource == entity; } );
    }

    // What changed without changing what plays.
    void Apply( Entity entity )
    {
        const auto sourceMaybe = mWorld.GetScene().TryGet<AudioSource>( entity );
        if ( not sourceMaybe )
            return;
        for ( const Scope<Voice>& voice : mVoices )
            if ( voice->mSource == entity )
                Configure( *voice, *sourceMaybe );
    }

    static void Configure( Voice& voice, const AudioSource& source )
    {
        voice.mVolume = source.mVolume;
        voice.mMinDistance = source.mMinDistance;
        voice.mMaxDistance = source.mMaxDistance;
        ma_sound& sound = voice.Sound();
        ma_sound_set_volume( &sound, source.mVolume );
        ma_sound_set_pitch( &sound, source.mPitch );
        ma_sound_set_looping( &sound, source.mLoop ? MA_TRUE : MA_FALSE );
        ma_sound_set_min_distance( &sound, source.mMinDistance );
        ma_sound_set_max_distance( &sound, source.mMaxDistance );
    }

    vec3 PositionOf( Entity entity ) const
    {
        const auto worldMaybe = mWorld.GetScene().TryGet<WorldTransform>( entity );
        return worldMaybe ? vec3( worldMaybe->mMatrix[3] ) : vec3( ComputeWorldMatrix( mWorld.GetScene(), entity )[3] );
    }

    // How loud a voice is where the listener is: what gives way when the
    // world has no room.
    f32 Loudness( f32 volume, bool spatial, vec3 position, f32 minDistance, f32 maxDistance ) const
    {
        if ( not spatial )
            return volume;
        const f32 distance = glm::clamp( glm::length( position - mListener ), minDistance, maxDistance );
        return volume * minDistance / max( distance, 0.0001f );
    }

    void StartWaiting()
    {
        for ( size_t at = 0; at < mWaiting.size(); )
        {
            Waiting& waiting = mWaiting[at];
            const AssetState state = waiting.mAssetRef.State();
            if ( state == AssetState::Loading )
            {
                ++at;
                continue;
            }
            if ( state == AssetState::Failed )
                LogError( "audio: {}: {}", waiting.mAssetRef.Entry().Path().String(),
                          waiting.mAssetRef.Entry().Error() );
            else
                Start( waiting );
            mWaiting.erase( mWaiting.begin() + static_cast<ptrdiff_t>( at ) );
        }
    }

    void Start( const Waiting& waiting )
    {
        Scene& scene = mWorld.GetScene();
        const auto sourceMaybe =
            waiting.mSource == Entity::Null ? OptRef<AudioSource>() : scene.TryGet<AudioSource>( waiting.mSource );
        if ( waiting.mSource != Entity::Null and not sourceMaybe )
            return;
        ma_sound_group* group = mDevice.GetImpl().TryGroup( waiting.mBus );
        if ( group == nullptr )
        {
            LogError( "audio: no bus {} (buses: {})", waiting.mBus.ToString(), mDevice.BusNames() );
            return;
        }
        const bool spatial = sourceMaybe ? sourceMaybe->mSpatial : waiting.mPosition.has_value();
        const vec3 position = sourceMaybe ? PositionOf( waiting.mSource ) : waiting.mPosition.value_or( vec3( 0.0f ) );
        const f32 minDistance = sourceMaybe ? sourceMaybe->mMinDistance : 1.0f;
        const f32 maxDistance = sourceMaybe ? sourceMaybe->mMaxDistance : 50.0f;

        // No room: the quietest gives way, unless this would be quieter yet.
        if ( mVoices.size() >= cMaxVoices )
        {
            const auto quietest = ranges::min_element( mVoices, {}, [&]( const Scope<Voice>& voice ) {
                return Loudness( voice->mVolume, voice->mSpatial, voice->mPosition, voice->mMinDistance,
                                 voice->mMaxDistance );
            } );
            const f32 loudest = Loudness( ( *quietest )->mVolume, ( *quietest )->mSpatial, ( *quietest )->mPosition,
                                          ( *quietest )->mMinDistance, ( *quietest )->mMaxDistance );
            if ( Loudness( waiting.mVolume, spatial, position, minDistance, maxDistance ) <= loudest )
                return;
            mVoices.erase( quietest );
        }

        auto voice = CreateScope<Voice>();
        if ( not voice->TryStart( mDevice.GetImpl().mEngine, *group, waiting.mAssetRef.TryGet()->mData, spatial ) )
        {
            LogError( "audio: {} does not play", waiting.mAssetRef.Entry().Path().String() );
            return;
        }
        voice->mSource = waiting.mSource;
        voice->mBus = waiting.mBus;
        voice->mSpatial = spatial;
        voice->mPosition = position;
        if ( sourceMaybe )
            Configure( *voice, *sourceMaybe );
        else
        {
            voice->mVolume = waiting.mVolume;
            ma_sound_set_volume( &voice->Sound(), waiting.mVolume );
        }
        if ( spatial )
            ma_sound_set_position( &voice->Sound(), position.x, position.y, position.z );
        if ( mWorld.Paused() and waiting.mBus != "ui"_n )
            voice->mHeld = true;
        else
            ma_sound_start( &voice->Sound() );
        mVoices.push_back( std::move( voice ) );
    }

    // Where the world is heard from: its listener, or its first camera.
    void PlaceListener()
    {
        const Scene& scene = mWorld.GetScene();
        opt<mat4> earsMaybe;
        for ( const Entity entity : scene.EntitiesWith<AudioListener>() )
            if ( scene.Get<AudioListener>( entity ).mActive )
            {
                earsMaybe = ComputeWorldMatrix( scene, entity );
                break;
            }
        if ( not earsMaybe )
            if ( const auto cameraMaybe = mWorld.GetApp().GetComponentTypes().TryFind( "camera"_n ) )
                for ( const Entity entity : scene.EntitiesWith<WorldTransform>() )
                    if ( cameraMaybe->mGet( scene, entity ) )
                    {
                        earsMaybe = scene.Get<WorldTransform>( entity ).mMatrix;
                        break;
                    }
        const mat4 ears = earsMaybe.value_or( mat4( 1.0f ) );
        mListener = vec3( ears[3] );
        const vec3 forward = glm::normalize( vec3( ears * vec4( 0.0f, 0.0f, -1.0f, 0.0f ) ) );
        const vec3 up = glm::normalize( vec3( ears * vec4( 0.0f, 1.0f, 0.0f, 0.0f ) ) );
        ma_engine& engine = mDevice.GetImpl().mEngine;
        ma_engine_listener_set_position( &engine, 0, mListener.x, mListener.y, mListener.z );
        ma_engine_listener_set_direction( &engine, 0, forward.x, forward.y, forward.z );
        ma_engine_listener_set_world_up( &engine, 0, up.x, up.y, up.z );
    }

    void Update( f32 dt )
    {
        BUBBLE_PROFILE_ZONE_NAMED( "Audio" );
        if ( not mAudible )
            return;
        PlaceListener();
        const bool paused = mWorld.Paused();
        for ( const Scope<Voice>& voice : mVoices )
        {
            if ( voice->mSource != Entity::Null and voice->mSpatial )
            {
                voice->mPosition = PositionOf( voice->mSource );
                ma_sound_set_position( &voice->Sound(), voice->mPosition.x, voice->mPosition.y, voice->mPosition.z );
            }
            // A paused world is silent but for its UI; it goes on where it
            // stopped.
            if ( paused and voice->mBus != "ui"_n and not voice->mHeld and ma_sound_is_playing( &voice->Sound() ) )
            {
                ma_sound_stop( &voice->Sound() );
                voice->mHeld = true;
            }
            else if ( not paused and voice->mHeld )
            {
                ma_sound_start( &voice->Sound() );
                voice->mHeld = false;
            }
        }
        // What ran to its end goes.
        erase_if( mVoices, []( const Scope<Voice>& voice ) {
            return not voice->mHeld and ma_sound_at_end( &voice->Sound() ) and
                   not ma_sound_is_looping( &voice->Sound() );
        } );
        for ( Waiting& waiting : mWaiting )
            waiting.mAge += dt;
        erase_if( mWaiting, []( const Waiting& waiting ) {
            return waiting.mSource == Entity::Null and waiting.mAge > cLongestWait;
        } );
        StartWaiting();
    }

    World& mWorld;
    AssetRegistry& mAssets;
    AudioDevice& mDevice;
    // In the order they started: the oldest first.
    vector<Scope<Voice>> mVoices;
    vector<Waiting> mWaiting;
    vec3 mListener{ 0.0f };
    bool mAudible = true;
};

// ---- what scripts get ---------------------------------------------------------

AudioWorld& AudioOf( World& world )
{
    return world.Get<AudioWorld>();
}

Name BusNamed( World& world, const opt<string>& busMaybe, Name fallback )
{
    if ( not busMaybe )
        return fallback;
    const auto nameMaybe = Name::TryFrom( *busMaybe );
    AudioDevice& device = AudioOf( world ).Device();
    if ( not nameMaybe or not device.HasBus( *nameMaybe ) )
        throw LuaError( format( "no audio bus \"{}\" (buses: master, {})", *busMaybe, device.BusNames() ) );
    return *nameMaybe;
}

void ScriptsOfAudio( ScriptApi& api )
{
    api.ExtendComponent(
        Name::Declare( "audio_source" ),
        []( LuaTypeBuilder<ScriptComponent>& type, World& world ) {
            type.Method( "play", [&world]( ScriptComponent& self ) { (void)AudioOf( world ).TryPlay( self.mEntity ); } )
                .Method( "stop", [&world]( ScriptComponent& self ) { (void)AudioOf( world ).TryStop( self.mEntity ); } )
                .Property( "playing",
                           [&world]( ScriptComponent& self ) { return AudioOf( world ).Playing( self.mEntity ); } );
        },
        "    function play(self): ()\n"
        "    function stop(self): ()\n"
        "    playing: boolean\n" );

    api.AddGlobals(
        []( LuaState& lua, World& world ) {
            LuaTable audio = lua.NewTable();
            audio["play_sound"] = [&world]( const AssetId& sound, opt<f32> volumeMaybe, opt<string> busMaybe ) {
                AudioOf( world ).PlaySound( sound, volumeMaybe.value_or( 1.0f ), BusNamed( world, busMaybe, "sfx"_n ),
                                            nullopt );
            };
            audio["play_sound_at"] = [&world]( const AssetId& sound, vec3 position, opt<f32> volumeMaybe ) {
                AudioOf( world ).PlaySound( sound, volumeMaybe.value_or( 1.0f ), "sfx"_n, position );
            };
            audio["set_bus_volume"] = [&world]( const string& bus, f32 volume ) {
                AudioOf( world ).Device().SetBusVolume( BusNamed( world, bus, {} ), volume );
            };
            audio["bus_volume"] = [&world]( const string& bus ) {
                return AudioOf( world ).Device().BusVolume( BusNamed( world, bus, {} ) );
            };
            lua.Globals()["audio"] = audio;
        },
        "declare audio: {\n"
        "    play_sound: (sound: asset, volume: number?, bus: string?) -> (),\n"
        "    play_sound_at: (sound: asset, position: vector, volume: number?) -> (),\n"
        "    set_bus_volume: (bus: string, volume: number) -> (),\n"
        "    bus_volume: (bus: string) -> number,\n"
        "}\n" );
}
}

ModuleType AudioModule( AssetRegistry& assets, AudioDevice& device )
{
    return ModuleType{
        .mName = "audio",
        .mDescribe =
            []( Reflection& reflection, ComponentTypes& componentTypes ) {
                reflection.Type<AudioSource>( "audio_source" )
                    .Field<&AudioSource::mSound>( "sound", { .mAssetKind = cSoundKind.data() } )
                    .Field<&AudioSource::mBus>( "bus" )
                    .Field<&AudioSource::mVolume>( "volume", { .mMin = 0, .mMax = 2, .mFlags = FieldInfo::Slider } )
                    .Field<&AudioSource::mPitch>( "pitch" )
                    .Field<&AudioSource::mLoop>( "loop" )
                    .Field<&AudioSource::mAutoplay>( "autoplay" )
                    .Field<&AudioSource::mSpatial>( "spatial" )
                    .Field<&AudioSource::mMinDistance>( "min_distance" )
                    .Field<&AudioSource::mMaxDistance>( "max_distance" );
                reflection.Type<AudioListener>( "audio_listener" ).Field<&AudioListener::mActive>( "active" );
                componentTypes.Add<AudioSource>();
                componentTypes.Add<AudioListener>();
            },
        .mCreate = [&assets, &device]( World& world ) -> Scope<Module> {
            return CreateScope<Audio>( world, assets, device );
        },
        .mScripts = &ScriptsOfAudio,
    };
}
}
