#pragma once
#include <sol/forward.hpp>

namespace bubble
{
struct Loader;

// The Sound usertype, play_sound and the master volume. Takes the loader
// because play_sound is addressed by path - play_sound( "sounds/step.mp3" ) -
// and resolves it through the same cache load_sound fills.
void CreateAudioBindings( Loader& loader, sol::state& lua );
}
