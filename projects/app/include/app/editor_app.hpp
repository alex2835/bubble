#pragma once
#include "engine/window/window.hpp"
#include "engine/renderer/buffer.hpp"
#include "engine/renderer/shader.hpp"
#include "engine/renderer/camera_free.hpp"
#include "engine/renderer/framebuffer.hpp"
#include "engine/renderer/renderer.hpp"
#include "engine/loader/loader.hpp"
#include "engine/serialization/event_serialization.hpp"
#include "engine/scene/scene.hpp"
#include "engine/utils/filesystem.hpp"
#include "scene_camera.hpp"

namespace bubble
{
class BubbleEditor
{
public:
    BubbleEditor();
    void Run();
private:
    Window mWindow;
    SceneCamera mSceneCamera;
    Timer mTimer;
    Scene mScene;
    Loader loader;
    Renderer mRenderer;
};

}
