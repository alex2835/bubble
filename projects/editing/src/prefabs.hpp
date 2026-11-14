#pragma once
#include "bubble/editing/workspace.hpp"

// A prefab written by the editor, and what follows from it in what is open.
namespace bubble
{
// The prefab's file, and the workspace's copy, are `prefab` from now on. The
// instances of it in the open documents are made again from it, what each
// changed against the prefab as it was kept, and a document of the prefab
// itself other than `skip` is made again whole. A document so changed
// forgets its history: its steps name entities that went.
expected<void, string> TryWritePrefab( Workspace& workspace,
                                       const AssetPath& path,
                                       Ref<const Prefab> prefab,
                                       OptRef<const Document> skip );

// A prefab's document saved to `path`, a .prefab: its nodes keep their ids,
// new ones get theirs, and what is open follows.
expected<void, string> TrySavePrefabDocument( Workspace& workspace, Document& document, const AssetPath& path );

// A document of the prefab at `path`, opened and active.
expected<void, string> TryOpenPrefabDocument( Workspace& workspace, const AssetPath& path );

bool IsPrefabPath( const AssetPath& path );
}
