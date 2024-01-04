#include "engine/engine.hpp"

namespace bubble
{
void Engine::OnUpdate()
{
    mTimer.OnUpdate();
    auto dt = mTimer.GetDeltaTime();
}

}
