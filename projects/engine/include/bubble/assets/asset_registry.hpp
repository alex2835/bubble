#pragma once
#include "bubble/assets/asset_id.hpp"
#include "bubble/core/asset_path.hpp"
#include "bubble/types/containers.hpp"
#include "bubble/types/function.hpp"
#include "bubble/types/handle.hpp"
#include "bubble/types/opt_ref.hpp"
#include "bubble/types/pointer.hpp"
#include "bubble/types/utility.hpp"

namespace bubble
{
class AssetRegistry;
class AssetEntryBase;

namespace detail
{
// Where the registry finds entries, by path and by id. Weak: an entry lives
// as long as its refs do, and takes itself out of the index when it goes.
struct AssetIndex
{
    hmap<string, WeakRef<AssetEntryBase>> mByPath;
    hmap<AssetId, WeakRef<AssetEntryBase>> mById;
};
}

enum class AssetState
{
    Loading,
    Ready,
    Failed,
};

// One asset in memory, shared by every ref to it. A reload puts the new
// version into the same entry, so refs taken before see it.
class AssetEntryBase
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
    AssetEntryBase( AssetId id, AssetPath path, type_index type ) : mId( id ), mPath( std::move( path ) ), mType( type )
    {
    }
    ~AssetEntryBase();

    AssetId mId;
    AssetPath mPath;
    type_index mType;
    AssetState mState = AssetState::Loading;
    string mError;
    u32 mVersion = 0;
    // Empty for an entry the registry does not list (a file asked for as the
    // wrong kind), and once the registry is gone.
    WeakRef<detail::AssetIndex> mIndex;
};

template <typename T>
class AssetEntry : public AssetEntryBase
{
public:
    AssetEntry( AssetId id, AssetPath path ) : AssetEntryBase( id, std::move( path ), typeid( T ) ) {}

private:
    friend class AssetRegistry;
    template <typename>
    friend class AssetRef;
    opt<T> mData;
};

// What a component's runtime holds: a counted reference to an entry, which
// stays while any ref to it does. Unlike a Handle it never goes stale. Get()
// is empty until the asset is Ready - code checks rather than waits, because
// loading will not always finish at once. What Get() returns is good until
// the next reload; keep the AssetRef, not the reference.
template <typename T>
class AssetRef
{
public:
    AssetRef() = default;

    explicit operator bool() const { return mEntry != nullptr; }
    AssetState State() const { return mEntry ? mEntry->State() : AssetState::Failed; }
    bool Ready() const { return State() == AssetState::Ready; }
    OptRef<const T> Get() const
    {
        if ( not Ready() )
            return nullopt;
        return *mEntry->mData;
    }
    const AssetEntryBase& Entry() const
    {
        if ( mEntry == nullptr )
            throw logic_error( "Entry() of an empty asset ref" );
        return *mEntry;
    }

    friend bool operator==( const AssetRef& a, const AssetRef& b ) { return a.mEntry == b.mEntry; }

private:
    friend class AssetRegistry;
    explicit AssetRef( Ref<AssetEntry<T>> entry ) : mEntry( std::move( entry ) ) {}
    Ref<AssetEntry<T>> mEntry;
};

using AssetListenerHandle = Handle<struct AssetListenerTag>;

// Every asset of the project in memory, one entry each, by path and by id.
// One per process, shared by its worlds. Importers turn a file's bytes into
// the asset by its extension.
//
// For now loading is synchronous and imports from the source file on the
// spot; later it reads the import cache in the background and Load returns
// a ref that is Loading. The calls stay the same, so code written against
// this one already checks State.
class AssetRegistry
{
public:
    // The bytes of a project file: from a directory, from memory in tests,
    // from fetch on the web.
    using ReadFile = function<expected<string, string>( const AssetPath& path )>;
    // A file's bytes into the asset.
    template <typename T>
    using Importer = function<expected<T, string>( string_view bytes, const AssetPath& path )>;
    using ChangedListener = function<void( const AssetEntryBase& entry )>;

    explicit AssetRegistry( ReadFile read );
    // Reads files under `root`.
    static ReadFile FromDirectory( OsPath root );

    // `kind` names it in errors: "script", "texture".
    template <typename T>
    void RegisterImporter( string_view extension, string_view kind, Importer<T> import )
    {
        mImporters.insert_or_assign(
            string( extension ),
            RegisteredImporter{
                typeid( T ), string( kind ),
                [import = std::move( import )]( string_view bytes, AssetEntryBase& entry ) -> expected<void, string> {
                    auto asset = import( bytes, entry.Path() );
                    if ( not asset )
                        return unexpected( std::move( asset.error() ) );
                    static_cast<AssetEntry<T>&>( entry ).mData.emplace( std::move( *asset ) );
                    return {};
                } } );
    }

    // The asset at `path`; the same entry when it is in memory already. A
    // file that is missing, of an unknown kind or fails to import gives a
    // Failed ref with the reason in Error(); whoever asked reports it.
    template <typename T>
    AssetRef<T> Load( const AssetPath& path )
    {
        return AssetRef<T>( static_pointer_cast<AssetEntry<T>>( LoadEntry(
            path, typeid( T ), [&] { return CreateRef<AssetEntry<T>>( AssetId::MakeFrom( path.View() ), path ); } ) ) );
    }

    // The asset if it is in memory - never loads.
    template <typename T>
    AssetRef<T> Find( const AssetPath& path ) const
    {
        return Typed<T>( FindEntry( path ) );
    }

    template <typename T>
    AssetRef<T> Find( const AssetId& id ) const
    {
        const auto found = mIndex->mById.find( id );
        return Typed<T>( found == mIndex->mById.end() ? nullptr : found->second.lock() );
    }

    // How many assets are in memory.
    size_t Count() const { return mIndex->mByPath.size(); }

    // The file changed: imports it again into its entry and tells the
    // listeners. If the new version fails, the old one stays and the error
    // comes back. A file nobody holds is not loaded, and nothing happens.
    expected<void, string> Reload( const AssetPath& path );

    // Called after every reload that took, with the entry.
    AssetListenerHandle OnChanged( ChangedListener listener );
    void RemoveListener( AssetListenerHandle listener );

private:
    struct RegisteredImporter
    {
        type_index mType;
        string mKind;
        function<expected<void, string>( string_view bytes, AssetEntryBase& entry )> mImport;
    };

    template <typename T>
    static AssetRef<T> Typed( Ref<AssetEntryBase> entry )
    {
        if ( entry == nullptr or entry->mType != type_index( typeid( T ) ) )
            return {};
        return AssetRef<T>( static_pointer_cast<AssetEntry<T>>( std::move( entry ) ) );
    }

    Ref<AssetEntryBase> LoadEntry( const AssetPath& path,
                                   type_index type,
                                   const function<Ref<AssetEntryBase>()>& make );
    Ref<AssetEntryBase> FindEntry( const AssetPath& path ) const;
    expected<void, string> Import( AssetEntryBase& entry );

    ReadFile mRead;
    hmap<string, RegisteredImporter> mImporters;
    // Shared with the entries, which may outlive the registry.
    Ref<detail::AssetIndex> mIndex = CreateRef<detail::AssetIndex>();
    SlotMap<ChangedListener, AssetListenerTag> mListeners;
};
}
