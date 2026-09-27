#pragma once
// Only the base. Pulling components.hpp in here put every component header
// under every file that touches the scene, so editing one component rebuilt
// half the engine. A file that uses a component includes that component.
#include "engine/scene/components/component_base.hpp"
#include <recs/registry.hpp>

namespace bubble
{
using namespace recs;
class Level;

// The entities of a level or a prefab. They form one tree: every entity has a
// HierarchyComponent, and every one but the root has a parent - the root
// being the level itself (or the prefab), which the Entities window shows at
// the top. See engine/scene/hierarchy.hpp.
class Scene : public Registry
{
public:
    Scene();
    friend Level; // serialization reaches into the registry

    Entity Root() const { return mRoot; }
    void SetRoot( Entity root ) { mRoot = root; }

private:
    Entity mRoot = INVALID_ENTITY;
};

}

