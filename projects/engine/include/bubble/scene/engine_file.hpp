#pragma once
#include "bubble/assets/asset_catalog.hpp"
#include "bubble/data/json.hpp"
#include "bubble/scene/component_types.hpp"
#include "bubble/types/function.hpp"
#include "bubble/types/pointer.hpp"

namespace bubble
{
class Prefab;

// Where prefabs come from when a level is read or written: by id, read and
// parsed - and cached, if whoever gives them likes.
using PrefabSource = function<expected<Ref<const Prefab>, string>( const AssetId& prefab )>;

// What a level's or a prefab's file is read and written with besides its
// scene: the components it may hold, the catalog its links to assets go
// through, and where its instances' prefabs come from - without them an
// instance is an error.
struct FileContext
{
    FileContext( const ComponentTypes& componentTypes, const AssetCatalog& assets, PrefabSource prefabs = {} )
        : mComponentTypes( componentTypes ),
          mAssets( assets ),
          mPrefabs( std::move( prefabs ) )
    {
    }

    const ComponentTypes& mComponentTypes;
    const AssetCatalog& mAssets;
    PrefabSource mPrefabs;
};

// The files the engine writes itself: levels and prefabs.
constexpr array<string_view, 2> cEngineFileExtensions = { ".scene", ".prefab" };

// A level and a prefab are assets the engine writes itself, so each keeps
// its own id inside rather than in an .import: "asset" at the top of the
// file. Nothing for a file without one - written by hand, never saved by
// the editor; the project's scan gives it one. A file that is no JSON
// object, or an id that is no id, is an error.
expected<opt<AssetId>, string> TryReadFileId( string_view text );
// The file with `assetId` as its own, the rest as it was.
expected<string, string> TryWithFileId( string_view text, const AssetId& assetId );
// What a level and a prefab write at the top: "version", then "asset" when
// the id is not zero.
void WriteFileHead( Json::Object& file, int version, const AssetId& assetId );
}
