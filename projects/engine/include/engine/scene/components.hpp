#pragma once
#include "glm/glm.hpp"
#include <string>
#include <algorithm>

#include "engine/loader/loader.h"
#include "engine/renderer/light.h"

namespace Bubble
{
// ================= Tag Component =================
struct TagComponent
{
    std::string mTag;

    TagComponent();
    TagComponent( const TagComponent& ) = default;
    TagComponent( const std::string& tag );

    //void Serialize( const Loader& loader, nlohmann::json& out ) const;
    //void Deserialize( const nlohmann::json& j, Loader& loader );
};

// ================= Position Component =================
struct PositionComponent
{
    glm::vec3 mPosition;

    PositionComponent() = default;
    PositionComponent( const glm::vec3& mPosition );

    //void Serialize( const Loader& loader, nlohmann::json& out ) const;
    //void Deserialize( const nlohmann::json& j, Loader& loader );
};

// ================= Rotation Component =================
struct RotationComponent
{
    glm::vec3 mRotation;

    RotationComponent() = default;
    RotationComponent( const glm::vec3& rotation );

    //void Serialize( const Loader& loader, nlohmann::json& out ) const;
    //void Deserialize( const nlohmann::json& j, Loader& loader );
};

// ================= ScaleComponent =================
struct ScaleComponent
{
    glm::vec3 mScale = glm::vec3( 1.0f );

    ScaleComponent() = default;
    ScaleComponent( const glm::vec3& scale );

    //void Serialize( const Loader& loader, nlohmann::json& out ) const;
    //void Deserialize( const nlohmann::json& j, Loader& loader );
};

// ================= Transform Component =================
struct TransformComponent
{
    glm::mat4 mTransform = glm::mat4( 1.0f );

    TransformComponent() = default;
    TransformComponent( const TransformComponent& ) = default;
    TransformComponent( glm::mat4 transform );

    operator glm::mat4& ( );
    operator const glm::mat4& ( ) const;

    //void Serialize( const Loader& loader, nlohmann::json& out ) const;
    //void Deserialize( const nlohmann::json& j, Loader& loader );
};

// ================= Light Component =================
struct LightComponent : Light
{
    //void Serialize( const Loader& loader, nlohmann::json& out ) const;
    //void Deserialize( const nlohmann::json& j, Loader& loader );
};

// ================= Model Component =================
struct ModelComponent : Ref<Model>
{
    ModelComponent() = default;
    ModelComponent( const Ref<Model>& model );

    operator Ref<Model>& ( );
    operator const Ref<Model>& ( ) const;

    //void Serialize( const Loader& loader, nlohmann::json& out ) const;
    //void Deserialize( const nlohmann::json& j, Loader& loader );
};

}