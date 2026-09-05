#pragma once
#include <sol/forward.hpp>

namespace bubble
{
class Scene;
class PhysicsEngine;
struct Loader;

// The `spawn` global: one entity from one table. Split out of the scene
// bindings because the table it accepts, and the per-field validation that
// comes with it, is most of what either file contains.
void CreateSpawnBindings( Scene& scene,
                          Loader& loader,
                          PhysicsEngine& physicsEngine,
                          sol::state& lua );
}
