#pragma once
#include "components.hpp"
#include <recs/registry.hpp>

namespace bubble
{
using namespace recs;
class Level;

class Scene : public Registry
{
public:
    Scene();
    friend Level; // serialization reaches into the registry
};

}

