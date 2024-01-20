#pragma once
#include <string>
#include "engine/utils/types.hpp"
#include "engine/utils/timer.hpp"
#include "engine/renderer/framebuffer.hpp"

namespace bubble
{
class IEditorInterface
{
public:
    virtual ~IEditorInterface() = default;
    virtual const string& Name() = 0;
    virtual void OnInit() = 0;
    virtual void OnUpdate( DeltaTime dt ) = 0;
    virtual void OnDraw() = 0;
protected:
    string mName;
    bool mOpen = true;
};
}
