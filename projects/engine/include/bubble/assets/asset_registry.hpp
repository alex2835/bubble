#pragma once
#include "bubble/assets/asset_id.hpp"
#include "bubble/core/asset_path.hpp"
#include "bubble/types/containers.hpp"
#include "bubble/types/handle.hpp"
#include "bubble/types/opt_ref.hpp"
#include "bubble/types/pointer.hpp"
#include <functional>
#include <typeindex>

namespace bubble
{
class AssetRegistry;

enum class AssetState
{
    Loading,
    Ready,
    Failed,
};

// One asset in memory, shared by every handle to it. A reload puts the new
// version into the same slot, so handles taken before see it.
class AssetSlotBase
{
public:
    const AssetId& Id() const { return mId; }
    const AssetPath& Path() const { return mPath; }
    AssetState State() const { return mState; }
    // Why it failed; empty otherwise.
    const string& Error() const { return mError; }
    // Grows with every reload that took.
    u32 Version() const { return mVersion; }

protected:
    friend class AssetRegistry;
    AssetSlotBase( AssetId id, AssetPath path, std::type_index type )
        : mId( id ),
          mPath( std::move( path ) ),
          mType( type )
    {
    }

    AssetId mId;
    AssetPath mPath;
    std::type_index mType;
    AssetState mState = AssetState::Loading;
    string mError;
    u32 mVersion = 0;
};

template <typename T>
class AssetSlot : public AssetSlotBase
{
public:
    AssetSlot( AssetId id, AssetPath path ) : AssetSlotBase( id, std::move( path ), typeid( T ) ) {}

private:
    friend class AssetRegistry;
    template <typename>
    friend class AssetHandle;
    opt<T> mData;
};

// What a component's runtime holds: a counted reference to a slot. The
// slot stays while any handle to it does. Get() is empty until the asset is
// Ready - code checks rather than waits, because loading will not always
// finish at once. What Get() returns is good until the next reload; keep
// the handle, not the reference.
template <typename T>
class AssetHandle
{
public:
    AssetHandle() = default;

    explicit operator bool() const { return mSlot != nullptr; }
    AssetState State() const { return mSlot ? mSlot->State() : AssetState::Failed; }
    bool Ready() const { return State() == AssetState::Ready; }
    OptRef<const T> Get() const
    {
        if ( not Ready() )
            return std::nullopt;
        return *mSlot->mData;
    }
    const AssetSlotBase& Slot() const { return *mSlot; }

    friend bool operator==( const AssetHandle& a, const AssetHandle& b ) { return a.mSlot == b.mSlot; }

private:
    friend class AssetRegistry;
    explicit AssetHandle( Ref<AssetSlot<T>> slot ) : mSlot( std::move( slot ) ) {}
    Ref<AssetSlot<T>> mSlot;
};

using AssetListenerHandle = Handle<struct AssetListenerTag>;

// Every asset of the project in memory, one slot each, by path and by id.
// One per process, shared by its worlds. Importers turn a file's bytes into
// the asset by its extension.
//
// For now loading is synchronous and imports from the source file on the
// spot; later it reads the import cache in the background and Load returns
// a handle that is Loading. The calls stay the same, so code written against
// this one already checks State.
class AssetRegistry
{
public:
    // The bytes of a project file: from a directory, from memory in tests,
    // from fetch on the web.
    using ReadFile = std::function<expected<string, string>( const AssetPath& path )>;
    // A file's bytes into the asset.
    template <typename T>
    using Importer = std::function<expected<T, string>( string_view bytes, const AssetPath& path )>;
    using ChangedListener = std::function<void( const AssetSlotBase& slot )>;

    explicit AssetRegistry( ReadFile read );
    // Reads files under `root`.
    static ReadFile FromDirectory( std::filesystem::path root );

    // `kind` names it in errors: "script", "texture".
    template <typename T>
    void RegisterImporter( string_view extension, string_view kind, Importer<T> import )
    {
        mImporters.insert_or_assign(
            string( extension ),
            ImporterEntry{
                typeid( T ), string( kind ),
                [import = std::move( import )]( string_view bytes, AssetSlotBase& slot ) -> expected<void, string> {
                    auto asset = import( bytes, slot.Path() );
                    if ( not asset )
                        return std::unexpected( std::move( asset.error() ) );
                    static_cast<AssetSlot<T>&>( slot ).mData.emplace( std::move( *asset ) );
                    return {};
                } } );
    }

    // The asset at `path`; the same slot when it is in memory already. A
    // file that is missing, of an unknown kind or fails to import gives a
    // Failed handle with the reason in Error(); whoever asked reports it.
    template <typename T>
    AssetHandle<T> Load( const AssetPath& path )
    {
        return AssetHandle<T>( std::static_pointer_cast<AssetSlot<T>>( LoadSlot(
            path, typeid( T ), [&] { return CreateRef<AssetSlot<T>>( AssetId::MakeFrom( path.View() ), path ); } ) ) );
    }

    // The asset if it is in memory - never loads.
    template <typename T>
    AssetHandle<T> Find( const AssetPath& path ) const
    {
        return Typed<T>( FindSlot( path ) );
    }

    template <typename T>
    AssetHandle<T> Find( const AssetId& id ) const
    {
        const auto found = mById.find( id );
        return Typed<T>( found == mById.end() ? nullptr : found->second.lock() );
    }

    // The file changed: imports it again into its slot and tells the
    // listeners. If the new version fails, the old one stays and the error
    // comes back. A file nobody holds is not loaded, and nothing happens.
    expected<void, string> Reload( const AssetPath& path );

    // Called after every reload that took, with the slot.
    AssetListenerHandle OnChanged( ChangedListener listener );
    void RemoveListener( AssetListenerHandle listener );

private:
    struct ImporterEntry
    {
        std::type_index mType;
        string mKind;
        std::function<expected<void, string>( string_view bytes, AssetSlotBase& slot )> mImport;
    };

    template <typename T>
    static AssetHandle<T> Typed( Ref<AssetSlotBase> slot )
    {
        if ( slot == nullptr or slot->mType != std::type_index( typeid( T ) ) )
            return {};
        return AssetHandle<T>( std::static_pointer_cast<AssetSlot<T>>( std::move( slot ) ) );
    }

    Ref<AssetSlotBase> LoadSlot( const AssetPath& path, std::type_index type,
                                 const std::function<Ref<AssetSlotBase>()>& make );
    Ref<AssetSlotBase> FindSlot( const AssetPath& path ) const;
    expected<void, string> Import( AssetSlotBase& slot );

    ReadFile mRead;
    str_hmap<ImporterEntry> mImporters;
    // Weak: a slot lives as long as its handles do.
    str_hmap<std::weak_ptr<AssetSlotBase>> mByPath;
    hmap<AssetId, std::weak_ptr<AssetSlotBase>> mById;
    SlotMap<ChangedListener, AssetListenerTag> mListeners;
};
}
