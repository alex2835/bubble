#pragma once
#include "bubble/types/string.hpp"

namespace bubble
{
// One of the engine's own WGSL files, projects/engine/shaders/<name>, as it
// was built into the engine. One it does not have is a logic_error.
string_view EngineShader( string_view name );
}
