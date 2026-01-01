#pragma once
#include <sol/forward.hpp>
#include "engine/physics/physics_engine.hpp"

namespace bubble
{
void CreatePhysicsBindings( PhysicsEngine& physicsEngine, sol::state& lua );
}
