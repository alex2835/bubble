#pragma once
#include "bubble/types/string.hpp"

namespace bubble
{
class AssetRegistry;

// A .luau file as the engine keeps it. Import will add what it reads from
// the AST: the props the file declares and the files it requires.
struct ScriptAsset
{
    string mBytecode;
};

// Teaches the registry .luau files. For now it compiles on the spot; with
// the import cache it moves to bubble_import, and the runner reads bytecode.
void RegisterScriptImporter( AssetRegistry& registry );
}
