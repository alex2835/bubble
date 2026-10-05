#include "bubble/assets/asset_registry.hpp"
#include <format>
#include <fstream>
#include <sstream>

namespace bubble
{
AssetRegistry::AssetRegistry( ReadFile read ) : mRead( std::move( read ) )
{
}

AssetRegistry::ReadFile AssetRegistry::FromDirectory( std::filesystem::path root )
{
    return [root = std::move( root )]( const AssetPath& path ) -> expected<string, string> {
        std::ifstream file( path.ToOsPath( root ), std::ios::binary );
        if ( not file )
            return std::unexpected( "no such file"s );
        std::ostringstream bytes;
        bytes << file.rdbuf();
        return std::move( bytes ).str();
    };
}

Ref<AssetSlotBase> AssetRegistry::FindSlot( const AssetPath& path ) const
{
    const auto found = mByPath.find( path.View() );
    return found == mByPath.end() ? nullptr : found->second.lock();
}

Ref<AssetSlotBase> AssetRegistry::LoadSlot( const AssetPath& path, std::type_index type,
                                            const std::function<Ref<AssetSlotBase>()>& make )
{
    if ( Ref<AssetSlotBase> slot = FindSlot( path ) )
    {
        if ( slot->mType == type )
            return slot;
        // The same file asked for as another kind: a failed handle of the
        // kind asked, beside the one in memory.
        Ref<AssetSlotBase> wrong = make();
        wrong->mState = AssetState::Failed;
        wrong->mError = std::format( "{} is loaded as another kind of asset", path.View() );
        return wrong;
    }

    Ref<AssetSlotBase> slot = make();
    mByPath[path.String()] = slot;
    mById[slot->mId] = slot;
    if ( auto imported = Import( *slot ); not imported )
    {
        slot->mState = AssetState::Failed;
        slot->mError = std::move( imported.error() );
        return slot;
    }
    slot->mState = AssetState::Ready;
    return slot;
}

expected<void, string> AssetRegistry::Import( AssetSlotBase& slot )
{
    const string_view extension = slot.mPath.Extension();
    const auto importer = mImporters.find( extension );
    if ( importer == mImporters.end() )
        return std::unexpected( std::format( "no importer for '{}' files", extension ) );
    if ( importer->second.mType != slot.mType )
        return std::unexpected( std::format( "a {} file, asked for as something else", importer->second.mKind ) );
    auto bytes = mRead( slot.mPath );
    if ( not bytes )
        return std::unexpected( std::move( bytes.error() ) );
    return importer->second.mImport( *bytes, slot );
}

expected<void, string> AssetRegistry::Reload( const AssetPath& path )
{
    const Ref<AssetSlotBase> slot = FindSlot( path );
    if ( slot == nullptr )
        return {};
    // An importer fills the slot only when it succeeds: a failed import
    // leaves the old version in place.
    if ( auto imported = Import( *slot ); not imported )
    {
        // A slot that never loaded keeps saying why.
        if ( slot->mState != AssetState::Ready )
            slot->mError = imported.error();
        return imported;
    }
    slot->mState = AssetState::Ready;
    slot->mError.clear();
    ++slot->mVersion;
    for ( const AssetListenerHandle listener : mListeners.Handles() )
        if ( const auto found = mListeners.Get( listener ) )
        {
            // A copy: a listener may take itself off while it runs.
            const ChangedListener call = *found;
            call( *slot );
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
