#pragma once
#include "bubble/types/string.hpp"

namespace bubble
{
class AssetRegistry;

// A .luau file as the engine keeps it: its bytecode, which the importer
// compiled. Import will add what it reads from the AST: the props the file
// declares and the files it requires.
struct ScriptAsset
{
    string mBytecode;
};

constexpr string_view cScriptKind = "script";

// Teaches the registry scripts: imported bytecode, <id>.script.
void RegisterScriptLoader( AssetRegistry& registry );
}
