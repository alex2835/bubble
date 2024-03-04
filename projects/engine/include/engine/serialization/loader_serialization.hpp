#pragma once
#include "engine/loader/loader.hpp"

namespace bubble
{
void to_json( json& j, const Loader& loader );
void from_json( const json& j, Loader& loader );
}
