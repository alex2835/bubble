#pragma once
#include "bubble/assets/asset_catalog.hpp"
#include "bubble/import/importer.hpp"
#include "bubble/import/project_files.hpp"
#include "bubble/scripts/script_declarations.hpp"
#include "bubble/types/containers.hpp"

namespace bubble
{
// What importing a script takes besides its source.
struct ScriptCheck
{
    // bubble.d.luau (WriteScriptDeclarations): what scripts see of the
    // engine. Given, every script is type-checked strictly; empty, scripts
    // are compiled and read but not checked - the runtime's own tests.
    string mDeclarations;
    // The project's .luaurc aliases, "lib" -> "scripts/lib": what requires
    // resolve through.
    hmap<string, string> mAliases;
    // A project file's text: the libraries a script requires are checked
    // with their own source.
    function<expected<string, string>( const AssetPath& path )> mRead;
    // The asset at a path, for a prop's asset( "models/hero.glb" ) default;
    // an empty one when there is none, or no way to ask.
    function<AssetId( const AssetPath& path )> mAssetOf;
};

// .luau into a ScriptAsset, kind "script": compiled; its props read from
// its props { ... } - each default a literal the inspector can show: a
// number, string or boolean, vector.create( x, y, z ), color( r, g, b ),
// asset( "path" ), entity(), prop( default, hints ), a table of them, an
// empty list as {} :: { T }; its requires, string literals, resolved from
// the file. Given declarations, its types checked as the doc's «Только
// типизированный код» says: strict whatever the file's header says
// (--!nonstrict and --!nocheck are errors), no `any` written, the
// callbacks' parameters annotated. Any of it wrong refuses the file with
// the path and line first: "scripts/player.luau:3: ...".
Importer ScriptImporter( ScriptCheck check = {} );

// The aliases of a .luaurc: { "aliases": { "lib": "scripts/lib" } }.
expected<hmap<string, string>, string> TryReadLuaurc( string_view text );

// How a project's scripts are checked: App's declarations with the
// project's labels, the aliases of the .luaurc at its root - none without
// one - its files read from `files`, its assets found in `catalog`; both
// outlive the importer. A .luaurc that does not read is an error.
expected<ScriptCheck, string> TryProjectScriptCheck( App& app,
                                                     const ProjectFiles& files,
                                                     const AssetCatalog& catalog,
                                                     const ScriptLabels& labels = {} );
}
