#pragma once
#include <sol/forward.hpp>

namespace bubble
{
class Scene;
class PhysicsEngine;
struct Loader;

void CreateSceneBindings( Scene& scene,
                          Loader& loader,
                          PhysicsEngine& physicsEngine,
                          sol::state& lua );
}
