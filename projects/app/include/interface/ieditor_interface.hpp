#pragma once
#include <string>
#include "engine/utils/timer.hpp"

namespace bubble
{
class IEditorInterface
{
public:
    virtual ~IEditorInterface() = default;
    virtual std::string Name() = 0;
    virtual void OnInit() = 0;
    virtual void OnUpdate() = 0;
    virtual void OnDraw() = 0;
private:
};
}
