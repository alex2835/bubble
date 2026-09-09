#include "engine/pch/pch.hpp"
#include "engine/loader/loader.hpp"
#include "engine/audio/audio_engine.hpp"
#include "engine/scripting/bindings/loader_lua_bindings.hpp"
#include "binding_utils.hpp"
#include <sol/sol.hpp>

namespace bubble
{
void CreateLoaderBindings( Loader& loader, sol::state& lua )
{
  lua["load_texture"] = [&]( const string& str ) { return loader.LoadTexture2D( str ); };
  lua["load_model"] = [&]( const string& str ) { return loader.LoadModel( str ); };
  lua["load_shader"] = [&]( const string& str ) { return loader.LoadShader( str ); };
  lua["load_script"] = [&]( const string& str ) { return loader.LoadScript( str ); };
  lua["load_sound"] = [&]( const string& str ) { return loader.LoadSound( str ); };

  lua.new_usertype<Sound>(
      "Sound",
      "name", &Sound::mName,
      "streaming", &Sound::mStreaming,
      sol::meta_function::to_string,
      []( const Sound& sound ) { return sound.mName; }
  );

  // Fire and forget. An AudioSourceComponent is one voice, so retriggering it
  // cuts off whatever it was playing - which is right for a looping engine hum
  // and wrong for footsteps. This is the other half: every call is its own
  // voice, freed by the engine when it runs out.
  //
  // With a position it is spatialized, without one it is not, because a 3D
  // sound at the origin is the single most confusing way to get silence.
  lua["play_sound"] = sol::overload(
      [&]( const string& soundPath )
      {
          auto sound = LoadOrThrow<Ref<Sound>>( [&]( const path& p ){ return loader.LoadSound( p ); },
                                                "sound", soundPath );
          AudioEngine::Get().Play( sound, VoiceParams{ .mSpatialized = false } );
      },
      [&]( const string& soundPath, const vec3& position )
      {
          auto sound = LoadOrThrow<Ref<Sound>>( [&]( const path& p ){ return loader.LoadSound( p ); },
                                                "sound", soundPath );
          AudioEngine::Get().Play( sound, VoiceParams{ .mSpatialized = true, .mPosition = position } );
      },
      [&]( const string& soundPath, const vec3& position, f32 volume )
      {
          auto sound = LoadOrThrow<Ref<Sound>>( [&]( const path& p ){ return loader.LoadSound( p ); },
                                                "sound", soundPath );
          AudioEngine::Get().Play( sound, VoiceParams{ .mVolume = volume,
                                                       .mSpatialized = true,
                                                       .mPosition = position } );
      }
  );

  lua["set_master_volume"] = []( f32 volume ) { AudioEngine::Get().SetMasterVolume( volume ); };
  lua["get_master_volume"] = []() { return AudioEngine::Get().GetMasterVolume(); };
}
}
