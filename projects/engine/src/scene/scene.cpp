#include "engine/pch/pch.hpp"
#include "engine/scene/scene.hpp"
#include "engine/scene/component_manager.hpp"
#include <sol/sol.hpp>
#include "engine/scene/components/audio_listener_component.hpp"
#include "engine/scene/components/audio_source_component.hpp"
#include "engine/scene/components/camera_component.hpp"
#include "engine/scene/components/character_controller_component.hpp"
#include "engine/scene/components/light_component.hpp"
#include "engine/scene/components/model_component.hpp"
#include "engine/scene/components/rigid_body_component.hpp"
#include "engine/scene/components/script_component.hpp"
#include "engine/scene/components/shader_component.hpp"
#include "engine/scene/components/state_component.hpp"
#include "engine/scene/components/tag_component.hpp"
#include "engine/scene/components/transform_component.hpp"

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
    AddComponent<RigidBodyComponent>();
    AddComponent<CharacterControllerComponent>();
    AddComponent<StateComponent>();
    AddComponent<AudioSourceComponent>();
    AddComponent<AudioListenerComponent>();

    ComponentManager::Add<TagComponent>();
    ComponentManager::Add<ModelComponent>();
    ComponentManager::Add<TransformComponent>();
    ComponentManager::Add<ShaderComponent>();
    ComponentManager::Add<CameraComponent>();
    ComponentManager::Add<LightComponent>();
    ComponentManager::Add<ScriptComponent>();
    ComponentManager::Add<RigidBodyComponent>();
    ComponentManager::Add<CharacterControllerComponent>();
    ComponentManager::Add<StateComponent>();
    ComponentManager::Add<AudioSourceComponent>();
    ComponentManager::Add<AudioListenerComponent>();
}

}