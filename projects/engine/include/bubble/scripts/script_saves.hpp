#pragma once
#include "bubble/assets/asset_id.hpp"
#include "bubble/types/containers.hpp"
#include "bubble/types/filesystem.hpp"
#include "bubble/types/function.hpp"
#include "bubble/types/pointer.hpp"
#include "bubble/types/string.hpp"

namespace bubble
{
class Jobs;

// What a game remembers from one level to the next: the scripts'
// `progress`, kept as bytes between worlds. A world's scripts start from
// what the last world left and leave theirs when they go.
class ScriptProgress
{
public:
    // Empty: a new game.
    const string& Bytes() const { return mBytes; }
    void Keep( string bytes ) { mBytes = std::move( bytes ); }

private:
    string mBytes;
};

// The game's save slots, by name: each whole, or not there. Where they live
// is the platform's - a folder of the user's on the desktop, the browser's
// storage on the web, memory in tests.
class SaveSlots
{
public:
    using Done = function<void( expected<void, string> written )>;

    virtual ~SaveSlots() = default;
    // Replaces the slot whole: a write cut short leaves the old one. Off
    // the main thread where there is a pool; `done` is told on the main
    // thread.
    virtual void Write( string slot, string bytes, Done done ) = 0;
    // Nothing for a slot never written.
    virtual expected<opt<string>, string> TryRead( string_view slot ) const = 0;
    // By name.
    virtual vector<string> List() const = 0;
};

Ref<SaveSlots> SaveSlotsInMemory();
// <folder>/<slot>.save, written beside and renamed over; written on Jobs'
// pool, which outlives the slots.
Ref<SaveSlots> SaveSlotsInFolder( OsPath folder, Jobs& jobs );

// A slot's name as a file can carry it: letters, digits, _ and -.
expected<void, string> TryCheckSlotName( string_view slot );

// What a slot says before it is loaded: what a load menu lists.
struct SaveHeader
{
    // The game's, as it wrote the slot.
    string mVersion;
    // The level it was written in.
    AssetId mLevel;
    // Seconds since 1970, UTC.
    f64 mTime = 0;
};

// A slot's bytes: the header, then the scripts' data as luaubind encodes
// it. Bytes of another format are refused, not converted.
string WriteSave( const SaveHeader& header, string_view data );
expected<pair<SaveHeader, string>, string> TryReadSave( string_view bytes );
}
