#pragma once
#include <sol/forward.hpp>

namespace bubble
{
class Scene;

// The `for_each_entity` global. Its per-component dispatch grows with
// ComponentID, so it lives on its own rather than inside the scene bindings.
void CreateForEachBindings( Scene& scene, sol::state& lua );
}
