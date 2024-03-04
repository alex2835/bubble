#pragma once
#include "engine/scene/scene.hpp"

namespace recs
{
using namespace bubble;
void to_json( json& j, const Scene& scene );
void from_json( const json& j, Scene& scene );
}
