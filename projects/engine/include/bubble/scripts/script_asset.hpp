#pragma once
#include "bubble/data/value.hpp"
#include "bubble/types/containers.hpp"
#include "bubble/types/string.hpp"

namespace bubble
{
class AssetRegistry;

// A prop as the file declares it in props { ... }: read from the source at
// import, so the inspector shows a script's props with no Luau running.
struct ScriptProp
{
    Name mName;
    ValueType mType;
    Value mDefault;
    // From prop( default, { min = 0, max = 30, tooltip = "..." } ): what the
    // inspector's widget is given; both 0 is unbounded.
    f32 mMin = 0;
    f32 mMax = 0;
    string mTooltip;

    friend bool operator==( const ScriptProp& a, const ScriptProp& b )
    {
        return a.mName == b.mName and a.mDefault == b.mDefault and a.mMin == b.mMin and a.mMax == b.mMax and
               a.mTooltip == b.mTooltip and a.mType.Describe() == b.mType.Describe();
    }
};

// A .luau file as the engine keeps it: its bytecode, which the importer
// compiled, and what the importer read from its source - the props it
// declares and the files it requires, as paths from the project's root.
struct ScriptAsset
{
    string mBytecode;
    vector<ScriptProp> mProps;
    // What a world loads before the file runs: require itself never loads.
    vector<string> mRequires;
};

constexpr string_view cScriptKind = "script";

// The asset as the import cache keeps it, <id>.script, and back. Bytes of
// another format are refused, not converted.
string WriteScriptAsset( const ScriptAsset& asset );
expected<ScriptAsset, string> TryReadScriptAsset( string_view bytes );

// Teaches the registry scripts: imported bytecode, <id>.script.
void RegisterScriptLoader( AssetRegistry& registry );

// The file a require names, from the requiring file `from` - both paths
// from the project's root - as Luau's own require resolves it, so luau-lsp
// agrees: "./x" and "../x" from `from`'s directory, "@alias/x" through
// `aliases` (alias -> directory, what .luaurc says); no extension, which
// is ".luau". A bare path, an unknown alias or one leading out of the
// project is an error saying so.
expected<string, string> TryResolveRequire( string_view from,
                                            string_view request,
                                            const hmap<string, string>& aliases );
}
