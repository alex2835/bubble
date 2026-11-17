// miniaudio ships as a single header; this is its one implementation unit.
// Same pattern as deps/stb_image.
//
// Built as C, not C++: miniaudio is C99, and under Emscripten it must not be
// compiled with a -std=c* flag at all (it emits Web Audio JavaScript directly).
// Keeping it in its own translation unit also keeps ~96k lines out of every
// engine TU that only wants to start a sound.

// Playback only ever reads assets, so the WAV/FLAC encoders are dead weight.
// The decoders are untouched - they are what loads the assets.
#define MA_NO_ENCODING

#if defined(__APPLE__)
    // miniaudio resolves CoreAudio at runtime by default, which trips Apple's
    // notarization (it needs allow-dyld-environment-variables entitlements).
    // Linking the frameworks up front avoids the entitlement entirely; see the
    // macOS notes in miniaudio.h and mackron/miniaudio#203.
    #define MA_NO_RUNTIME_LINKING
#endif

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
