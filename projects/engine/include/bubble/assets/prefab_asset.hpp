#pragma once
#include "bubble/types/string.hpp"

namespace bubble
{
class AssetRegistry;

// A .prefab as the registry keeps it: its text, as the project has it.
// Parsed where it is used, with the component types it needs
// (Prefab::TryParse) - spawn parses it once a version.
struct PrefabAsset
{
    string mText;
};

constexpr string_view cPrefabKind = "prefab";

// Teaches the registry prefabs: <id>.prefab in the import cache.
void RegisterPrefabLoader( AssetRegistry& registry );
}
