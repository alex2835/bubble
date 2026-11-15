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

AssetRegistry::AssetRegistry( const AssetCatalog& catalog, ReadImported read )
    : mCatalog( catalog ),
      mRead( std::move( read ) )
{
}

AssetRegistry::ReadImported AssetRegistry::FromDirectory( OsPath root )
{
    return [root = std::move( root )]( const AssetId& assetId, string_view kind ) -> expected<string, string> {
        ifstream file( ImportedPath( assetId, kind ).ToOsPath( root ), ios::binary );
        if ( not file )
            return unexpected( "no such file"s );
        ostringstream bytes;
        bytes << file.rdbuf();
        return std::move( bytes ).str();
    };
}

Ref<AssetEntryBase> AssetRegistry::TryFindEntry( const AssetId& assetId ) const
{
    const auto found = mIndex->mById.find( assetId );
    return found == mIndex->mById.end() ? nullptr : found->second.lock();
}

Ref<AssetEntryBase> AssetRegistry::LoadEntry( const AssetId& assetId,
                                              const AssetPath& path,
                                              type_index type,
                                              MakeEntry make )
{
    if ( Ref<AssetEntryBase> entry = TryFindEntry( assetId ) )
    {
        if ( entry->mType == type )
            return entry;
        // The same file asked for as another kind: a failed ref of the kind
        // asked, beside the one in memory.
        Ref<AssetEntryBase> wrong = make( assetId, path );
        wrong->mState = AssetState::Failed;
        wrong->mError = format( "{} is loaded as another kind of asset", path.View() );
        return wrong;
    }

    Ref<AssetEntryBase> entry = make( assetId, path );
    mIndex->mById[assetId] = entry;
    entry->mIndex = mIndex;
    if ( auto importedMaybe = TryLoadBytes( *entry ); not importedMaybe )
    {
        entry->mState = AssetState::Failed;
        entry->mError = std::move( importedMaybe.error() );
        return entry;
    }
    entry->mState = AssetState::Ready;
    return entry;
}

expected<void, string> AssetRegistry::TryLoadBytes( AssetEntryBase& entry )
{
    const auto loader = mLoaders.find( entry.mType );
    if ( loader == mLoaders.end() )
        throw logic_error(
            format( "AssetRegistry: {} asked for as a type no loader is registered for", entry.mPath.View() ) );
    auto bytesMaybe = mRead( entry.mId, loader->second.mKind );
    if ( not bytesMaybe )
        return unexpected( format( "no imported {}: {}", loader->second.mKind, bytesMaybe.error() ) );
    return loader->second.mLoad( *bytesMaybe, entry );
}

expected<void, string> AssetRegistry::TryReload( const AssetPath& path )
{
    const auto assetIdMaybe = mCatalog.TryIdOf( path );
    const Ref<AssetEntryBase> entry = assetIdMaybe ? TryFindEntry( *assetIdMaybe ) : nullptr;
    if ( entry == nullptr )
        return {};
    // A loader fills the entry only when it succeeds: a failed load leaves
    // the old version in place.
    if ( auto importedMaybe = TryLoadBytes( *entry ); not importedMaybe )
    {
        // An entry that never loaded keeps saying why.
        if ( entry->mState != AssetState::Ready )
            entry->mError = importedMaybe.error();
        return importedMaybe;
    }
    entry->mState = AssetState::Ready;
    entry->mError.clear();
    ++entry->mVersion;
    for ( const AssetListenerHandle listenerHandle : mListeners.Handles() )
        if ( const auto foundMaybe = mListeners.TryGet( listenerHandle ) )
        {
            // A copy: a listener may take itself off while it runs.
            const ChangedListener call = *foundMaybe;
            call( *entry );
        }
    return {};
}

AssetListenerHandle AssetRegistry::OnChanged( ChangedListener listener )
{
    return mListeners.Add( std::move( listener ) );
}

void AssetRegistry::RemoveListener( AssetListenerHandle listenerHandle )
{
    mListeners.Remove( listenerHandle );
}
}
