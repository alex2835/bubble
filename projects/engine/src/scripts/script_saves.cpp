#include "bubble/scripts/script_saves.hpp"
#include "bubble/core/jobs.hpp"
#include "bubble/core/utf8.hpp"
#include "bubble/data/binary.hpp"
#include "bubble/types/algorithm.hpp"
#include "bubble/types/format.hpp"
#include "bubble/types/stream.hpp"
#include "bubble/types/thread.hpp"
#include <cstring>

namespace bubble
{
namespace
{
constexpr string_view cMagic = "BSAV";
constexpr u32 cFormat = 1;
constexpr string_view cExtension = ".save";
constexpr string_view cWriting = ".save.writing";

class MemorySlots final : public SaveSlots
{
public:
    void Write( string slot, string bytes, Done done ) override
    {
        mSlots[std::move( slot )] = std::move( bytes );
        if ( done )
            done( {} );
    }

    expected<opt<string>, string> TryRead( string_view slot ) const override
    {
        const auto found = mSlots.find( string( slot ) );
        return found == mSlots.end() ? opt<string>() : opt<string>( found->second );
    }

    vector<string> List() const override
    {
        vector<string> names;
        for ( const auto& [name, bytes] : mSlots )
            names.push_back( name );
        ranges::sort( names );
        return names;
    }

private:
    hmap<string, string> mSlots;
};

class FolderSlots final : public SaveSlots, public enable_shared_from_this<FolderSlots>
{
public:
    FolderSlots( OsPath folder, Jobs& jobs ) : mFolder( std::move( folder ) ), mJobs( jobs ) {}

    void Write( string slot, string bytes, Done done ) override
    {
        // Held by the work: slots let go of while a write runs stay till
        // it is done.
        mJobs.Background( [self = shared_from_this(), slot = std::move( slot ), bytes = std::move( bytes ),
                           done = std::move( done )]() mutable {
            auto writtenMaybe = self->TryWriteNow( slot, bytes );
            self->mJobs.RunOnMain( [done = std::move( done ), writtenMaybe = std::move( writtenMaybe )] {
                if ( done )
                    done( writtenMaybe );
            } );
        } );
    }

    expected<opt<string>, string> TryRead( string_view slot ) const override
    {
        const lock_guard lock( mWriting );
        const OsPath path = PathOf( slot );
        error_code error;
        if ( not fs::exists( path, error ) )
            return opt<string>();
        ifstream file( path, ios::binary );
        if ( not file )
            return unexpected( format( "save slot {}: cannot read it", slot ) );
        ostringstream bytes;
        bytes << file.rdbuf();
        return opt<string>( std::move( bytes ).str() );
    }

    vector<string> List() const override
    {
        const lock_guard lock( mWriting );
        vector<string> names;
        error_code error;
        for ( fs::directory_iterator walk( mFolder, error ); not error and walk != fs::directory_iterator();
              walk.increment( error ) )
        {
            const string name = PathToUtf8( walk->path().filename() );
            if ( name.ends_with( cExtension ) and not name.ends_with( cWriting ) )
                names.push_back( name.substr( 0, name.size() - cExtension.size() ) );
        }
        ranges::sort( names );
        return names;
    }

private:
    OsPath PathOf( string_view slot ) const { return mFolder / PathFromUtf8( string( slot ) + string( cExtension ) ); }

    expected<void, string> TryWriteNow( const string& slot, const string& bytes )
    {
        const lock_guard lock( mWriting );
        const OsPath target = PathOf( slot );
        const OsPath beside = mFolder / PathFromUtf8( slot + string( cWriting ) );
        error_code error;
        fs::create_directories( mFolder, error );
        if ( error )
            return unexpected( format( "save slot {}: cannot make its folder: {}", slot, error.message() ) );
        {
            ofstream file( beside, ios::binary | ios::trunc );
            file.write( bytes.data(), static_cast<streamsize>( bytes.size() ) );
            file.close();
            if ( not file )
                return unexpected( format( "save slot {}: cannot write it", slot ) );
        }
        fs::rename( beside, target, error );
        if ( error )
        {
            const string why = error.message();
            fs::remove( beside, error );
            return unexpected( format( "save slot {}: cannot put it in place: {}", slot, why ) );
        }
        return {};
    }

    OsPath mFolder;
    Jobs& mJobs;
    // One write at a time, and no read in the middle of one.
    mutable mutex mWriting;
};
}

Ref<SaveSlots> SaveSlotsInMemory()
{
    return CreateRef<MemorySlots>();
}

Ref<SaveSlots> SaveSlotsInFolder( OsPath folder, Jobs& jobs )
{
    return CreateRef<FolderSlots>( std::move( folder ), jobs );
}

expected<void, string> TryCheckSlotName( string_view slot )
{
    const bool fits = not slot.empty() and slot.size() <= 64 and ranges::all_of( slot, []( char c ) {
        return ( c >= 'a' and c <= 'z' ) or ( c >= 'A' and c <= 'Z' ) or ( c >= '0' and c <= '9' ) or c == '_' or
               c == '-';
    } );
    if ( not fits )
        return unexpected( format( "save slot \"{}\": letters, digits, _ and -, at most 64", slot ) );
    return {};
}

string WriteSave( const SaveHeader& header, string_view data )
{
    BinaryWriter out;
    out.Bytes( cMagic );
    out.U32( cFormat );
    out.Text( header.mVersion );
    out.Text( header.mLevel ? header.mLevel.ToString() : string() );
    char time[sizeof( f64 )];
    std::memcpy( time, &header.mTime, sizeof( f64 ) );
    out.Bytes( string_view( time, sizeof( f64 ) ) );
    out.Text( data );
    return out.Take();
}

expected<pair<SaveHeader, string>, string> TryReadSave( string_view bytes )
{
    BinaryReader in( bytes );
    if ( in.TryBytes( cMagic.size() ) != cMagic )
        return unexpected( "not a save the game wrote"s );
    if ( const auto formatMaybe = in.TryU32(); formatMaybe != cFormat )
        return unexpected( format( "a save of format {}, this build reads {}", formatMaybe.value_or( 0 ), cFormat ) );
    SaveHeader header;
    const auto versionMaybe = in.TryText();
    const auto levelMaybe = versionMaybe ? in.TryText() : nullopt;
    const auto timeMaybe = levelMaybe ? in.TryBytes( sizeof( f64 ) ) : nullopt;
    const auto dataMaybe = timeMaybe ? in.TryText() : nullopt;
    if ( not dataMaybe or not in.AtEnd() )
        return unexpected( "the save is damaged"s );
    header.mVersion = string( *versionMaybe );
    if ( not levelMaybe->empty() )
    {
        const auto idMaybe = AssetId::TryParse( *levelMaybe );
        if ( not idMaybe )
            return unexpected( "the save is damaged"s );
        header.mLevel = *idMaybe;
    }
    std::memcpy( &header.mTime, timeMaybe->data(), sizeof( f64 ) );
    return pair<SaveHeader, string>( std::move( header ), string( *dataMaybe ) );
}
}
