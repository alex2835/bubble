#pragma once
#include <sol/forward.hpp>

namespace bubble
{
class Scene;
class PhysicsEngine;

void CreateSceneBindings( Scene& scene, 
                          PhysicsEngine& physicsEngine, 
                          sol::state& lua );
}
