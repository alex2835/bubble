#include "engine/pch/pch.hpp"
#include "engine/loader/loader.hpp"
#include "engine/audio/audio_engine.hpp"
#include "engine/scripting/bindings/audio_lua_bindings.hpp"
#include "binding_utils.hpp"
#include <sol/sol.hpp>

namespace bubble
{
void CreateAudioBindings( Loader& loader, sol::state& lua )
{
    lua.new_usertype<Sound>(
        "Sound",
        "name", &Sound::mName,
        "streaming", &Sound::mStreaming,
        sol::meta_function::to_string,
        []( const Sound& sound ) { return sound.mName; }
    );

    // Every overload goes through the loader's cache, so a path costs one
    // lookup after the first call, and a bad path fails here with the path in
    // the message rather than on the audio thread as silence.
    const auto loadSound = [&loader]( const string& soundPath )
    {
        return LoadOrThrow<Ref<Sound>>( [&]( const path& p ){ return loader.LoadSound( p ); },
                                        "sound", soundPath );
    };

    // Fire and forget. An AudioSourceComponent is one voice, so retriggering it
    // cuts off whatever it was playing - which is right for a looping engine hum
    // and wrong for footsteps. This is the other half: every call is its own
    // voice, freed by the engine when it runs out.
    //
    // With a position it is spatialized, without one it is not, because a 3D
    // sound at the origin is the single most confusing way to get silence.
    lua["play_sound"] = sol::overload(
        [loadSound]( const string& soundPath )
        {
            AudioEngine::Get().Play( loadSound( soundPath ), VoiceParams{ .mSpatialized = false } );
        },
        // 2D with a volume. The player's own footsteps in a third person game are
        // the canonical case: they are "yours" and should not fade with camera
        // distance, but they also should not sit at full volume under the music.
        [loadSound]( const string& soundPath, f32 volume )
        {
            AudioEngine::Get().Play( loadSound( soundPath ),
                                     VoiceParams{ .mVolume = volume, .mSpatialized = false } );
        },
        [loadSound]( const string& soundPath, const vec3& position )
        {
            AudioEngine::Get().Play( loadSound( soundPath ),
                                     VoiceParams{ .mSpatialized = true, .mPosition = position } );
        },
        [loadSound]( const string& soundPath, const vec3& position, f32 volume )
        {
            AudioEngine::Get().Play( loadSound( soundPath ),
                                     VoiceParams{ .mVolume = volume,
                                                  .mSpatialized = true,
                                                  .mPosition = position } );
        }
    );

    lua["set_master_volume"] = []( f32 volume ) { AudioEngine::Get().SetMasterVolume( volume ); };
    lua["get_master_volume"] = []() { return AudioEngine::Get().GetMasterVolume(); };
}
}
