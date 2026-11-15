#pragma once
#include "bubble/core/asset_path.hpp"
#include "bubble/data/json.hpp"
#include "bubble/types/containers.hpp"
#include "bubble/types/function.hpp"
#include "bubble/types/number.hpp"
#include "bubble/types/opt_ref.hpp"

namespace bubble
{
// A source file and its settings into the engine's format: a .luau into
// bytecode, a .png into a texture. What it makes goes to the import cache
// (ImportedPath), and the registry's loader of the same kind reads it.
struct Importer
{
    // The sources it takes: ".luau".
    string mExtension;
    // What it makes: "script". Names the imported file and its loader.
    string mKind;
    // Made larger when what it makes changes: every asset it made is
    // imported again.
    u32 mVersion = 1;
    // The source's bytes and the settings in its .import into what the
    // engine loads. The error says what is wrong with the source, its path
    // first: "scripts/player.luau:3: Expected ...".
    function<expected<string, string>( string_view source, const Json& settings, const AssetPath& path )> mImport;
};

// The importers a project has, one an extension.
class Importers
{
public:
    // An extension taken already is a logic_error.
    void Add( Importer importer );
    OptRef<const Importer> TryFind( string_view extension ) const;
    // What the project's scan takes for sources.
    vector<string_view> Extensions() const;

private:
    vector<Importer> mImporters;
};
}
