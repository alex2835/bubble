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
    friend Project;
};

}

