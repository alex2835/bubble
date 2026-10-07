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
    EraseExpired( index->mByPath, mPath.View() );
    EraseExpired( index->mById, mId );
}

AssetRegistry::AssetRegistry( ReadFile read ) : mRead( std::move( read ) )
{
}

AssetRegistry::ReadFile AssetRegistry::FromDirectory( OsPath root )
{
    return [root = std::move( root )]( const AssetPath& path ) -> expected<string, string> {
        ifstream file( path.ToOsPath( root ), ios::binary );
        if ( not file )
            return unexpected( "no such file"s );
        ostringstream bytes;
        bytes << file.rdbuf();
        return std::move( bytes ).str();
    };
}

Ref<AssetEntryBase> AssetRegistry::FindEntry( const AssetPath& path ) const
{
    const auto found = mIndex->mByPath.find( path.View() );
    return found == mIndex->mByPath.end() ? nullptr : found->second.lock();
}

Ref<AssetEntryBase> AssetRegistry::LoadEntry( const AssetPath& path, type_index type,
                                              const function<Ref<AssetEntryBase>()>& make )
{
    if ( Ref<AssetEntryBase> entry = FindEntry( path ) )
    {
        if ( entry->mType == type )
            return entry;
        // The same file asked for as another kind: a failed ref of the kind
        // asked, beside the one in memory.
        Ref<AssetEntryBase> wrong = make();
        wrong->mState = AssetState::Failed;
        wrong->mError = format( "{} is loaded as another kind of asset", path.View() );
        return wrong;
    }

    Ref<AssetEntryBase> entry = make();
    mIndex->mByPath[path.String()] = entry;
    mIndex->mById[entry->mId] = entry;
    entry->mIndex = mIndex;
    if ( auto imported = Import( *entry ); not imported )
    {
        entry->mState = AssetState::Failed;
        entry->mError = std::move( imported.error() );
        return entry;
    }
    entry->mState = AssetState::Ready;
    return entry;
}

expected<void, string> AssetRegistry::Import( AssetEntryBase& entry )
{
    const string_view extension = entry.mPath.Extension();
    const auto importer = mImporters.find( extension );
    if ( importer == mImporters.end() )
        return unexpected( format( "no importer for '{}' files", extension ) );
    if ( importer->second.mType != entry.mType )
        return unexpected( format( "a {} file, asked for as something else", importer->second.mKind ) );
    auto bytes = mRead( entry.mPath );
    if ( not bytes )
        return unexpected( std::move( bytes.error() ) );
    return importer->second.mImport( *bytes, entry );
}

expected<void, string> AssetRegistry::Reload( const AssetPath& path )
{
    const Ref<AssetEntryBase> entry = FindEntry( path );
    if ( entry == nullptr )
        return {};
    // An importer fills the entry only when it succeeds: a failed import
    // leaves the old version in place.
    if ( auto imported = Import( *entry ); not imported )
    {
        // An entry that never loaded keeps saying why.
        if ( entry->mState != AssetState::Ready )
            entry->mError = imported.error();
        return imported;
    }
    entry->mState = AssetState::Ready;
    entry->mError.clear();
    ++entry->mVersion;
    for ( const AssetListenerHandle listener : mListeners.Handles() )
        if ( const auto found = mListeners.Get( listener ) )
        {
            // A copy: a listener may take itself off while it runs.
            const ChangedListener call = *found;
            call( *entry );
        }
    return {};
}

AssetListenerHandle AssetRegistry::OnChanged( ChangedListener listener )
{
    return mListeners.Add( std::move( listener ) );
}

void AssetRegistry::RemoveListener( AssetListenerHandle listener )
{
    mListeners.Remove( listener );
}
}
