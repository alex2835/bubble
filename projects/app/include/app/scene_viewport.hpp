#pragma once
#include "engine/renderer/framebuffer.hpp"

namespace bubble
{

class SceneViewport : Framebuffer
{
public:

    SceneViewport() = default;

    void OnUpdate()
    {
        if( GetSize() != mSize )
            Resize( mSize );
    }

private:
    glm::uvec2 mSize;
};

}
