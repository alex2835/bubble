#pragma once
#include "engine/scene/components/component_base.hpp"

namespace bubble
{
// Where the player hears from. Position and orientation come from the entity's
// TransformComponent, so this carries no transform of its own - the same
// arrangement CameraComponent and LightComponent use.
//
// miniaudio supports several listeners, but the engine drives only the first
// one it finds with mActive set. More than one active listener is a scene
// authoring mistake rather than a feature, and is reported once per run.
struct AudioListenerComponent
{
    static int ID() { return static_cast<int>( ComponentID::AudioListener ); }
    static string_view Name() { return "audio_listener"sv; }

    // Fields for engine/reflection: saved, shown, set by path and bound to Lua.
    static void Reflect();

public:
    bool mActive = true;
};

}
