#pragma once
#include "engine/scene/scene.hpp"

namespace recs
{
using namespace bubble;
void SceneToJson( const Loader& loader, json& j, const Scene& scene );
void SceneFromJson( Loader& loader, const json& j, Scene& scene );
}
