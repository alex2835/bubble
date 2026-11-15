#include "bubble/assets/asset_registry.hpp"
#include "bubble/types/format.hpp"
#include "bubble/types/stream.hpp"

namespace bubble
{
namespace
{
// Takes `key` out of the index if what it points to is gone: a dying entry
// has expired by the time its destructor runs, a live one at the same key is
// another entry.
template <typename Map, typename Key>
void EraseExpired( Map& map, const Key& key )
{
    const auto found = map.find( key );
    if ( found != map.end() and found->second.expired() )
        map.erase( found );
}
}

AssetEntryBase::~AssetEntryBase()
{
    const Ref<detail::AssetIndex> index = mIndex.lock();
    if ( index == nullptr )
        return;
    EraseExpired( index->mById, mId );
}

AssetPath ImportedPath( const AssetId& assetId, string_view kind )
{
    return *AssetPath::TryFrom( format( ".bubble/imported/{}.{}", assetId.ToString(), kind ) );
}

namespace
{
using Install = function<void( AssetEntryBase& )>;
using LoadBytes = function<expected<Install, string>( string_view bytes, const AssetPath& path )>;

// What was read, loaded: wherever the read handed it over - so nothing here
// touches the entry or the registry.
expected<Install, string> TryLoad( expected<string, string> bytesMaybe,
                                   const string& kind,
                                   const LoadBytes& load,
                                   const AssetPath& path )
{
    if ( not bytesMaybe )
        return unexpected( format( "no imported {}: {}", kind, bytesMaybe.error() ) );
    return load( *bytesMaybe, path );
}

// Reads and loads, here and now: a read that hands its bytes over later is
// an error. What it hands over then goes nowhere.
expected<Install, string> TryReadHere( const AssetRegistry::ReadImported& read,
                                       const string& kind,
                                       const LoadBytes& load,
                                       const AssetId& assetId,
                                       const AssetPath& path )
{
    const auto handed = CreateRef<opt<expected<string, string>>>();
    read( assetId, kind, [handed]( expected<string, string> bytes ) { *handed = std::move( bytes ); } );
    if ( not *handed )
        return unexpected(
            format( "no imported {}: it is read later than now - LoadNow and reloads read at once", kind ) );
    return TryLoad( std::move( **handed ), kind, load, path );
}
}

AssetRegistry::AssetRegistry( const AssetCatalog& catalog, ReadImported read, Jobs& jobs )
    : mCatalog( catalog ),
      mRead( std::move( read ) ),
      mJobs( jobs )
{
}

AssetRegistry::ReadImported AssetRegistry::AtOnce( ReadAtOnce read )
{
    return [read = std::move( read )]( const AssetId& assetId, string_view kind, const Read& handOver ) {
        handOver( read( assetId, kind ) );
    };
}

AssetRegistry::ReadImported AssetRegistry::FromDirectory( OsPath root )
{
    return AtOnce( [root = std::move( root )]( const AssetId& assetId, string_view kind ) -> expected<string, string> {
        ifstream file( ImportedPath( assetId, kind ).ToOsPath( root ), ios::binary );
        if ( not file )
            return unexpected( "no such file"s );
        ostringstream bytes;
        bytes << file.rdbuf();
        return std::move( bytes ).str();
    } );
}

Ref<AssetEntryBase> AssetRegistry::TryFindEntry( const AssetId& assetId ) const
{
    const auto found = mIndex->mById.find( assetId );
    return found == mIndex->mById.end() ? nullptr : found->second.lock();
}

const AssetRegistry::RegisteredLoader& AssetRegistry::LoaderOf( const AssetEntryBase& entry ) const
{
    const auto loader = mLoaders.find( entry.mType );
    if ( loader == mLoaders.end() )
        throw logic_error(
            format( "AssetRegistry: {} asked for as a type no loader is registered for", entry.mPath.View() ) );
    return loader->second;
}

Ref<AssetEntryBase> AssetRegistry::LoadEntry( const AssetId& assetId,
                                              const AssetPath& path,
                                              type_index type,
                                              MakeEntry make,
                                              When when )
{
    if ( Ref<AssetEntryBase> entry = TryFindEntry( assetId ) )
    {
        if ( entry->mType != type )
        {
            // The same file asked for as another kind: a failed ref of the
            // kind asked, beside the one in memory.
            Ref<AssetEntryBase> wrong = make( assetId, path );
            wrong->mState = AssetState::Failed;
            wrong->mError = format( "{} is loaded as another kind of asset", path.View() );
            return wrong;
        }
        if ( when == When::Now and entry->mState == AssetState::Loading )
        {
            static_cast<void>( TryLoadHere( *entry ) );
            Tell( *mListeners, Heard::Loaded, *entry );
        }
        return entry;
    }

    Ref<AssetEntryBase> entry = make( assetId, path );
    mIndex->mById[assetId] = entry;
    entry->mIndex = mIndex;
    // Nobody has seen it Loading, so nobody is told it is not.
    if ( when == When::Now )
        static_cast<void>( TryLoadHere( *entry ) );
    else
        StartLoading( entry );
    return entry;
}

void AssetRegistry::StartLoading( const Ref<AssetEntryBase>& entry )
{
    const RegisteredLoader& loader = LoaderOf( *entry );
    // The pool gets copies and the entry by a weak ref only: the entry is
    // the main thread's, and so is its last ref going.
    // The read hands its bytes over where it likes - the pool, at once; the
    // main thread, when a fetch comes back - and they are loaded there.
    Read loadAndFinish =
        [kind = loader.mKind, load = loader.mLoad, path = entry->mPath, weakEntry = WeakRef<AssetEntryBase>( entry ),
         weakListeners = WeakRef<Listeners>( mListeners ), &jobs = mJobs]( expected<string, string> bytes ) {
            auto loadedMaybe = TryLoad( std::move( bytes ), kind, load, path );
            jobs.RunOnMain( [weakEntry, weakListeners, loadedMaybe = std::move( loadedMaybe )] {
                const Ref<AssetEntryBase> entry = weakEntry.lock();
                const Ref<Listeners> listeners = weakListeners.lock();
                // Nobody wants it any more, the registry went, or it was loaded
                // here and now meanwhile.
                if ( entry == nullptr or listeners == nullptr or entry->mState != AssetState::Loading )
                    return;
                Finish( *entry, loadedMaybe );
                Tell( *listeners, Heard::Loaded, *entry );
            } );
        };
    mJobs.Background( [read = mRead, kind = loader.mKind, assetId = entry->mId,
                       loadAndFinish = std::move( loadAndFinish )] { read( assetId, kind, loadAndFinish ); } );
}

expected<void, string> AssetRegistry::TryLoadHere( AssetEntryBase& entry )
{
    const RegisteredLoader& loader = LoaderOf( entry );
    const auto loadedMaybe = TryReadHere( mRead, loader.mKind, loader.mLoad, entry.mId, entry.mPath );
    Finish( entry, loadedMaybe );
    if ( not loadedMaybe )
        return unexpected( loadedMaybe.error() );
    return {};
}

void AssetRegistry::Finish( AssetEntryBase& entry, const expected<Install, string>& loaded )
{
    if ( not loaded )
    {
        entry.mState = AssetState::Failed;
        entry.mError = loaded.error();
        return;
    }
    ( *loaded )( entry );
    entry.mState = AssetState::Ready;
    entry.mError.clear();
}

void AssetRegistry::Tell( const Listeners& listeners, Heard heard, const AssetEntryBase& entry )
{
    for ( const AssetListenerHandle listenerHandle : listeners.mAll.Handles() )
        if ( const auto foundMaybe = listeners.mAll.TryGet( listenerHandle );
             foundMaybe and foundMaybe->mHeard == heard )
        {
            // A copy: a listener may take itself off while it runs.
            const Listener call = foundMaybe->mListener;
            call( entry );
        }
}

expected<void, string> AssetRegistry::TryReload( const AssetPath& path )
{
    const auto assetIdMaybe = mCatalog.TryIdOf( path );
    const Ref<AssetEntryBase> entry = assetIdMaybe ? TryFindEntry( *assetIdMaybe ) : nullptr;
    if ( entry == nullptr )
        return {};
    const RegisteredLoader& loader = LoaderOf( *entry );
    const auto loadedMaybe = TryReadHere( mRead, loader.mKind, loader.mLoad, entry->mId, entry->mPath );
    // A load fills the entry only when it succeeds: a failed one leaves the
    // old version in place, and an entry that never loaded keeps saying
    // why.
    if ( not loadedMaybe )
    {
        if ( entry->mState != AssetState::Ready )
            entry->mError = loadedMaybe.error();
        return unexpected( loadedMaybe.error() );
    }
    const bool first = entry->mState == AssetState::Loading;
    Finish( *entry, loadedMaybe );
    ++entry->mVersion;
    Tell( *mListeners, first ? Heard::Loaded : Heard::Changed, *entry );
    return {};
}

AssetListenerHandle AssetRegistry::OnLoaded( Listener listener )
{
    return mListeners->mAll.Add( Listening{ .mHeard = Heard::Loaded, .mListener = std::move( listener ) } );
}

AssetListenerHandle AssetRegistry::OnChanged( Listener listener )
{
    return mListeners->mAll.Add( Listening{ .mHeard = Heard::Changed, .mListener = std::move( listener ) } );
}

void AssetRegistry::RemoveListener( AssetListenerHandle listenerHandle )
{
    mListeners->mAll.Remove( listenerHandle );
}
}
