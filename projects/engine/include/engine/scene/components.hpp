#pragma once
#include <string>
#include <algorithm>
#include "engine/loader/loader.h"
#include "engine/renderer/light.h"

namespace Bubble
{
// ================= Tag Component =================
struct TagComponent
{
    string mTag;

    TagComponent();
    TagComponent( const TagComponent& ) = default;
    TagComponent( const string& tag );

    //void Serialize( const Loader& loader, nlohmann::json& out ) const;
    //void Deserialize( const nlohmann::json& j, Loader& loader );
};

// ================= Position Component =================
struct PositionComponent
{
    vec3 mPosition;

    PositionComponent() = default;
    PositionComponent( const vec3& mPosition );

    //void Serialize( const Loader& loader, nlohmann::json& out ) const;
    //void Deserialize( const nlohmann::json& j, Loader& loader );
};

// ================= Rotation Component =================
struct RotationComponent
{
    vec3 mRotation;

    RotationComponent() = default;
    RotationComponent( const vec3& rotation );

    //void Serialize( const Loader& loader, nlohmann::json& out ) const;
    //void Deserialize( const nlohmann::json& j, Loader& loader );
};

// ================= ScaleComponent =================
struct ScaleComponent
{
    vec3 mScale = vec3( 1.0f );

    ScaleComponent() = default;
    ScaleComponent( const vec3& scale );

    //void Serialize( const Loader& loader, nlohmann::json& out ) const;
    //void Deserialize( const nlohmann::json& j, Loader& loader );
};

// ================= Transform Component =================
struct TransformComponent
{
    mat4 mTransform = mat4( 1.0f );

    TransformComponent() = default;
    TransformComponent( const TransformComponent& ) = default;
    TransformComponent( mat4 transform );

    operator mat4& ( );
    operator const mat4& ( ) const;

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