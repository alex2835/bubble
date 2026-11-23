#pragma once
#include "bubble/core/asset_path.hpp"
#include "bubble/data/json.hpp"
#include "bubble/data/value.hpp"
#include "bubble/types/containers.hpp"
#include "bubble/types/function.hpp"
#include "bubble/types/number.hpp"
#include "bubble/types/opt_ref.hpp"

namespace bubble
{
// What an importer made of a source: the bytes of its kind, and of each kind
// it makes beside - a model's skeleton and clips -, by kind.
struct ImportedAsset
{
    // Not explicit: most importers make one kind and return its bytes.
    ImportedAsset( string bytes ) : mBytes( std::move( bytes ) ) {}

    string mBytes;
    // A kind of Importer::mMoreKinds the source has none of is left out.
    hmap<string, string> mMore;
};

// A setting a source's .import may hold, as the editor shows it: "srgb",
// "lods".
struct ImportSetting
{
    string mName;
    ValueType mType;
    // What the importer takes when the .import does not say; nothing when
    // the source decides - a sound streams when it is long.
    opt<Value> mDefault;
    // What it does, in a sentence.
    string mTooltip;
};

// A source file and its settings into the engine's format: a .luau into
// bytecode, a .png into a texture. What it makes goes to the import cache
// (ImportedPath), and the registry's loader of the same kind reads it.
struct Importer
{
    // The sources it takes: ".luau".
    string mExtension;
    // What it makes: "script". Names the imported file and its loader.
    string mKind;
    // What it may make beside, under the same id: "skeleton", "clips". Each
    // is written every import - empty when the source has none of it, which
    // its loader refuses -, so no file is left over from an earlier one.
    vector<string> mMoreKinds = {};
    // Made larger when what it makes changes: every asset it made is
    // imported again.
    u32 mVersion = 1;
    // What else what it makes follows from, besides the source and its
    // settings: the declarations scripts are checked against - the engine's
    // API, the project's actions. In the cache's key, so when it changes
    // every source it took is imported again.
    string mDependsOn = {};
    // The source's bytes and the settings in its .import into what the
    // engine loads. The error says what is wrong with the source, its path
    // first: "scripts/player.luau:3: Expected ...".
    function<expected<ImportedAsset, string>( string_view source, const Json& settings, const AssetPath& path )>
        mImport;
    // What its settings are, for the editor; the importer reads them itself.
    vector<ImportSetting> mSettings = {};
};

// The importers a project has, one an extension.
class Importers
{
public:
    // An extension taken already is a logic_error.
    void Add( Importer importer );
    // In place of the one for its extension; none there is a logic_error.
    void Replace( Importer importer );
    OptRef<const Importer> TryFind( string_view extension ) const;
    // What the project's scan takes for sources.
    vector<string_view> Extensions() const;

private:
    vector<Importer> mImporters;
};
}
