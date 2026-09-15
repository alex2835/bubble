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

class Scene : public Registry
{
public:
    Scene();
    friend Level; // serialization reaches into the registry
};

}

