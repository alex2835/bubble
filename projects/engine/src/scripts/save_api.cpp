#include "save_api.hpp"
#include "bubble/core/log.hpp"
#include "bubble/types/format.hpp"
#include "bubble/world/world.hpp"
#include <chrono>

namespace bubble
{
namespace
{
SaveSlots& SlotsOf( const Ref<SaveSlots>& slots, string_view what )
{
    if ( not slots )
        throw LuaError( format( "save.{}: this game keeps no save slots", what ) );
    return *slots;
}

void CheckSlot( string_view what, const string& slot )
{
    if ( auto checkedMaybe = TryCheckSlotName( slot ); not checkedMaybe )
        throw LuaError( format( "save.{}: {}", what, checkedMaybe.error() ) );
}

// The slot's header and data, or nothing for one never written.
opt<pair<SaveHeader, string>> TryOpen( const Ref<SaveSlots>& slots, string_view what, const string& slot )
{
    CheckSlot( what, slot );
    const auto bytesMaybe = SlotsOf( slots, what ).TryRead( slot );
    if ( not bytesMaybe )
        throw LuaError( format( "save.{}: {}", what, bytesMaybe.error() ) );
    if ( not *bytesMaybe )
        return nullopt;
    auto savedMaybe = TryReadSave( **bytesMaybe );
    if ( not savedMaybe )
        throw LuaError( format( "save.{}: slot {}: {}", what, slot, savedMaybe.error() ) );
    return std::move( *savedMaybe );
}
}

void RegisterSaves( LuaState& lua, World& world, Ref<SaveSlots> slots, string version )
{
    LuaTable save = lua.NewTable();
    // Written whole or not at all, off the main thread: a failure is
    // logged when it is known, the game going on meanwhile.
    save["write"] = [&lua, &world, slots, version]( const string& slot, const LuaTable& data ) {
        CheckSlot( "write", slot );
        SaveSlots& target = SlotsOf( slots, "write" );
        auto encodedMaybe = lua.TryEncode( data );
        if ( not encodedMaybe )
            throw LuaError( format( "save.write: {}", encodedMaybe.error() ) );
        const f64 now = std::chrono::duration<f64>( std::chrono::system_clock::now().time_since_epoch() ).count();
        const SaveHeader header{ .mVersion = version, .mLevel = world.Level(), .mTime = now };
        target.Write( slot, WriteSave( header, *encodedMaybe ), [slot]( expected<void, string> writtenMaybe ) {
            if ( not writtenMaybe )
                LogError( "save slot {} is not written: {}", slot, writtenMaybe.error() );
        } );
    };
    save["read"] = [&lua, slots]( const string& slot ) {
        const auto savedMaybe = TryOpen( slots, "read", slot );
        if ( not savedMaybe )
            return LuaValue();
        auto dataMaybe = lua.TryDecode( savedMaybe->second );
        if ( not dataMaybe )
            throw LuaError( format( "save.read: slot {}: {}", slot, dataMaybe.error() ) );
        return *dataMaybe;
    };
    save["info"] = [&lua, slots]( const string& slot ) {
        const auto savedMaybe = TryOpen( slots, "info", slot );
        if ( not savedMaybe )
            return LuaValue();
        LuaTable info = lua.NewTable();
        info["version"] = savedMaybe->first.mVersion;
        info["level"] = savedMaybe->first.mLevel;
        info["time"] = savedMaybe->first.mTime;
        return LuaValue( info );
    };
    save["list"] = [&lua, slots]() {
        LuaTable names = lua.NewTable();
        for ( const string& name : SlotsOf( slots, "list" ).List() )
            names.Append( lua.Value( name ) );
        return names;
    };
    lua.Globals()["save"] = save;
}
}
