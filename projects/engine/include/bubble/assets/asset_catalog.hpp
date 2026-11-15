#pragma once
#include "bubble/assets/asset_id.hpp"
#include "bubble/core/asset_path.hpp"
#include "bubble/types/containers.hpp"
#include "bubble/types/opt_ref.hpp"

namespace bubble
{
// Which asset is at which path. An asset is known by its id - components
// and files keep that - and found on disk by its path, which can change: the
// catalog is where one turns into the other. In the editor the project's
// scan fills it from the .import files; a cooked game will read it from the
// cook.
class AssetCatalog
{
public:
    // An id or a path the catalog has already is a logic_error.
    void Add( const AssetId& assetId, const AssetPath& path );
    // An id it does not have is left alone.
    void Remove( const AssetId& assetId );

    OptRef<const AssetPath> TryPathOf( const AssetId& assetId ) const;
    opt<AssetId> TryIdOf( const AssetPath& path ) const;
    size_t Count() const { return mPaths.size(); }
    // For messages: its path, or "asset 9c1e..." for one the catalog does
    // not have.
    string Describe( const AssetId& assetId ) const;

private:
    hmap<AssetId, AssetPath> mPaths;
    hmap<AssetPath, AssetId> mIds;
};
}
