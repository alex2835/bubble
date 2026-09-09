#pragma once
#include "engine/types/string.hpp"
#include "engine/utils/filesystem.hpp"

namespace bubble
{
// A sound asset. Deliberately thin: the PCM data itself is owned and shared by
// miniaudio's resource manager, which caches by path and refcounts, so two
// entities playing the same file decode it once without the Loader arranging
// anything.
//
// What this type carries is the *asset identity* - what the editor lists, what
// a scene serialises, what a script names. Loading one validates that the file
// exists and can actually be decoded, so a bad path fails when it is loaded
// rather than silently playing nothing three frames later.
struct Sound
{
    string mName;
    path mPath;
    // Streamed from disk rather than decoded up front. Music wants this; a
    // footstep does not, and paying a file read per shot would be worse.
    bool mStreaming = false;
};

}
