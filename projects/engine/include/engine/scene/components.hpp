#pragma once
#include <string>
#include <string_view>
#include <algorithm>
#include "engine/utils/types.hpp"
#include "engine/loader/loader.hpp"
#include "engine/renderer/light.hpp"

namespace bubble
{
// ================= Tag Component =================
struct TagComponent : string
{
    static string_view Name()
    {
        return "TagComponent";
    }
    //void Serialize( const Loader& loader, nlohmann::json& out ) const;
    //void Deserialize( const nlohmann::json& j, Loader& loader );
};

//// ================= Position Component =================
//struct PositionComponent
//{
//    vec3 mPosition;
//
//    PositionComponent() = default;
//    PositionComponent( const vec3& mPosition );
//
//    //void Serialize( const Loader& loader, nlohmann::json& out ) const;
//    //void Deserialize( const nlohmann::json& j, Loader& loader );
//};
//
//// ================= Rotation Component =================
//struct RotationComponent
//{
//    vec3 mRotation;
//
//    RotationComponent() = default;
//    RotationComponent( const vec3& rotation );
//
//    //void Serialize( const Loader& loader, nlohmann::json& out ) const;
//    //void Deserialize( const nlohmann::json& j, Loader& loader );
//};
//
//// ================= ScaleComponent =================
//struct ScaleComponent
//{
//    vec3 mScale = vec3( 1.0f );
//
//    ScaleComponent() = default;
//    ScaleComponent( const vec3& scale );
//
//    //void Serialize( const Loader& loader, nlohmann::json& out ) const;
//    //void Deserialize( const nlohmann::json& j, Loader& loader );
//};

// ================= Transform Component =================
struct TransformComponent : mat4
{
    static string_view Name()
    {
        return "TransformComponent";
    }

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
    static string_view Name()
    {
        return "ModelComponent";
    }

    //void Serialize( const Loader& loader, nlohmann::json& out ) const;
    //void Deserialize( const nlohmann::json& j, Loader& loader );
};

}