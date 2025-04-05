
#include "engine/scene/scene.hpp"
#include "engine/scene/component_manager.hpp"

namespace bubble
{
Scene::Scene()
{
    ComponentManager::Add<TagComponent>( *this );
    ComponentManager::Add<ModelComponent>( *this );
    ComponentManager::Add<TransformComponent>( *this );
    ComponentManager::Add<ShaderComponent>( *this );
    ComponentManager::Add<ScriptComponent>( *this );
    ComponentManager::Add<PhysicsComponent>( *this );
}

}