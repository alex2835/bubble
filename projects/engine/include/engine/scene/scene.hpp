#pragma once
#include "engine/utils/imexp.hpp"
#include "components.hpp"
#include <recs/registry.hpp>

namespace bubble
{
using namespace recs;
class BUBBLE_ENGINE_EXPORT Scene : public Registry
{
public:
    static void ToJson( const Scene& scene, const Loader& loader, json& j );
    static void FromJson( Scene& scene, Loader& loader, const json& j );
};

}

