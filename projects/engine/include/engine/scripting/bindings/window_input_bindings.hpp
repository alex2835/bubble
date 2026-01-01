#pragma once 
#include <sol/forward.hpp>
#include "engine/window/input.hpp"

namespace bubble
{
void CreateWindowInputBindings( WindowInput& input, sol::state& lua );
}
