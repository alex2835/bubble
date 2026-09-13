#pragma once
#include "engine/types/any.hpp"
#include "engine/types/json.hpp"

namespace bubble
{
class ScriptingEngine;

// Tagged JSON: scalars and tables map straight across, engine types carry a
// "__type" so they come back as the same usertype.
json SaveAnyValue( const Any& v );
Any LoadAnyValue( ScriptingEngine& se, const json& j );

}
