#pragma once
#include "components.hpp"
#include <recs/registry.hpp>

namespace bubble
{
using namespace recs;
class Scene : public Registry
{
public:
    Scene();
    void ToJson( const Loader& loader, json& j );
    void FromJson( Loader& loader, const json& j );
};

}

