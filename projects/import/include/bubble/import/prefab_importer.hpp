#pragma once
#include "bubble/assets/asset_catalog.hpp"
#include "bubble/import/importer.hpp"
#include "bubble/scene/component_types.hpp"

namespace bubble
{
// .prefab, kind "prefab": the file as it is, once it reads as a prefab of
// these component types - what spawn instantiates in a running world. The
// types and the catalog outlive the importer.
Importer PrefabImporter( const ComponentTypes& componentTypes, const AssetCatalog& catalog );
}
