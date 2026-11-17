#pragma once
// The device's insides, for the audio module: miniaudio, seen nowhere else.
#include "bubble/audio/audio.hpp"
#include <miniaudio.h>

namespace bubble
{
struct AudioDevice::Impl
{
    // In place: miniaudio's objects point into themselves.
    ma_engine mEngine{};
    vector<Name> mBuses;
    // By bus, under the engine's endpoint.
    vector<Scope<ma_sound_group>> mGroups;
    bool mOffline = false;
    bool mStarted = false;

    ~Impl();
    // Null for a bus there is not.
    ma_sound_group* TryGroup( Name bus );
};
}
