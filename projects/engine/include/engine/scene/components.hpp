#pragma once
#include <imgui.h>
#include <glm/gtx/matrix_decompose.hpp>
#include "glm/gtx/transform.hpp"
#include "glm/gtx/euler_angles.hpp"
#include "engine/utils/types.hpp"
#include "engine/loader/loader.hpp"
#include "engine/renderer/light.hpp"

namespace bubble
{
typedef void ( *OnComponentDrawFunc )( void* rawData );
struct Component
{
    OnComponentDrawFunc mOnDrawFunc = nullptr;
};

template <typename T>
concept DrawableCompnentType = requires( T comp, void* raw )
{
    { T::Name() } -> std::same_as<string_view>;
    { T::OnComponentDraw( raw ) } -> std::same_as<void>;
};

class BUBBLE_ENGINE_EXPORT ComponentsOnDrawStorage
{
public:
    template <DrawableCompnentType Component> 
    static void Add()
    {
        ComponentsOnDrawStorage::Add( string( Component::Name() ), Component::OnComponentDraw );;
    }

    template <DrawableCompnentType Component>
    static void Get()
    {
        return ComponentsOnDrawStorage::Get( Component::Name() );
    }

    static void Add( string componentName, OnComponentDrawFunc drawFunc );
    static OnComponentDrawFunc Get( string_view componentName );

private:
    ComponentsOnDrawStorage() = default;
    static ComponentsOnDrawStorage& Instance();

    strunomap<OnComponentDrawFunc> mOnDrawFuncCache;
};



// Basic components

struct TagComponent : public string
{
    static string_view Name()
    {
        return "TagComponent";
    }
    static void OnComponentDraw( void* raw )
    {
        auto& tag = *(TagComponent*)raw;
        char buffer[64] = { 0 };
        tag.copy( buffer, sizeof( buffer ) );
        ImGui::TextColored( ImVec4( 1, 1, 0, 1 ), "TagComponent" );
        ImGui::InputText( "##Tag", buffer, sizeof( buffer ) );
        tag.assign( buffer );
    }

    //void Serialize( const Loader& loader, nlohmann::json& out ) const;
    //void Deserialize( const nlohmann::json& j, Loader& loader );
};


struct TransformComponent
{
    static string_view Name()
    {
        return "TransformComponent";
    }
    static void OnComponentDraw( void* raw )
    {
        auto& component = *(TransformComponent*)raw;
        ImGui::TextColored( ImVec4( 1, 1, 0, 1 ), "TransformComponent" );
        ImGui::DragFloat3( "Scale", (float*)&component.mScale, 0.01f, 0.01f );
        ImGui::DragFloat3( "Rotation", (float*)&component.mRotation, 0.01f );
        ImGui::DragFloat3( "Position", (float*)&component.mPosition, 0.1f );
    }

    mat4 Transform()
    {
        auto transform = mat4( 1.0f );
        transform = glm::translate( transform, mPosition );
        transform = glm::rotate( transform, glm::radians( mRotation.x ), vec3( 1, 0, 0 ) );
        transform = glm::rotate( transform, glm::radians( mRotation.y ), vec3( 0, 1, 0 ) );
        transform = glm::rotate( transform, glm::radians( mRotation.z ), vec3( 0, 0, 1 ) );
        transform = glm::scale( transform, mScale );
        return transform;
    }

    vec3 mPosition = vec3( 0 );
    vec3 mRotation = vec3( 0 );
    vec3 mScale = vec3( 1 );
    //void Serialize( const Loader& loader, nlohmann::json& out ) const;
    //void Deserialize( const nlohmann::json& j, Loader& loader );
};

struct LightComponent : public Light
{
    static string_view Name()
    {
        return "LightComponent";
    }
    static void OnComponentDraw( void* raw )
    {
        auto& component = *(LightComponent*)raw;
        ImGui::TextColored( ImVec4( 1, 1, 0, 1 ), "LightComponent" );

    }
    //void Serialize( const Loader& loader, nlohmann::json& out ) const;
    //void Deserialize( const nlohmann::json& j, Loader& loader );
};

struct ModelComponent : public Ref<Model>
{
    static string_view Name()
    {
        return "ModelComponent";
    }
    static void OnComponentDraw( void* raw )
    {
        auto& component = *(ModelComponent*)raw;
        ImGui::TextColored( ImVec4( 1, 1, 0, 1 ), "ModelComponent" );
        ImGui::Text( component->mName.c_str() );
    }
    //void Serialize( const Loader& loader, nlohmann::json& out ) const;
    //void Deserialize( const nlohmann::json& j, Loader& loader );
};

}