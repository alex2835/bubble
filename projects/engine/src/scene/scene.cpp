#include "engine/pch/pch.hpp"
#include "engine/scene/scene.hpp"
#include "engine/scene/component_manager.hpp"

namespace bubble
{
Scene::Scene()
{
    AddComponent<TagComponent>();
    AddComponent<ModelComponent>();
    AddComponent<TransformComponent>();
    AddComponent<ShaderComponent>();
    AddComponent<CameraComponent>();
    AddComponent<LightComponent>();
    AddComponent<ScriptComponent>();
    AddComponent<PhysicsComponent>();
    AddComponent<StateComponent>();

    ComponentManager::Add<TagComponent>();
    ComponentManager::Add<ModelComponent>();
    ComponentManager::Add<TransformComponent>();
    ComponentManager::Add<ShaderComponent>();
    ComponentManager::Add<CameraComponent>();
    ComponentManager::Add<LightComponent>();
    ComponentManager::Add<ScriptComponent>();
    ComponentManager::Add<PhysicsComponent>();
    ComponentManager::Add<StateComponent>();
}

}