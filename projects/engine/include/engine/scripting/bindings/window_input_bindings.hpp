#pragma once 
#include <sol/forward.hpp>
#include "engine/window/input.hpp"

namespace bubble
{
class Window;

void CreateWindowInputBindings( WindowInput& input, sol::state& lua );
// Cursor control. Separate from the input bindings because it drives the
// window rather than reading it.
void CreateWindowBindings( Window& window, sol::state& lua );
}
