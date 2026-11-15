#pragma once
#include "bubble/assets/asset_id.hpp"
#include "bubble/scene/entity.hpp"
#include "bubble/types/number.hpp"

namespace bubble
{
// On the root of a prefab's instance: the prefab it is made from. Neither
// this nor PrefabLink is written as a component - a level file keeps an
// instance as its prefab and what differs from it - and the runtime never
// looks at them: an instance in memory is plain entities.
struct PrefabInstance
{
    AssetId mPrefab;
};

// On every entity made from a node of a prefab, its instance's root too:
// which instance, and which node - by the id that outlasts renames; the
// root's is 0.
struct PrefabLink
{
    Entity mInstance;
    u32 mNode = 0;
};
}
