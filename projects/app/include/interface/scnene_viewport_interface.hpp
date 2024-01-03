#pragma once
#include "interface/ieditor_interface.hpp"
#include "app/scene_viewport.hpp"

namespace bubble
{
class EditorViewportInterface : IEditorInterface
{
public:
    void OnInit() override
    {
    }


    void OnUpdate() override
    {
    }

    void OnDraw() override
    {
    }

private:
    SceneViewport mViewport;
};

}
