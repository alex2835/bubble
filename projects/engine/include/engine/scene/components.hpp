#pragma once
#include <imgui.h>
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

class BUBBLE_ENGINE_EXPORT OnComponentDrawFuncStorage
{
public:
    static OnComponentDrawFuncStorage& Instance();

    template <DrawableCompnentType Component> 
    void Add()
    {
        Add( string( Component::Name() ), Component::OnComponentDraw );
    }
    void Add( string componentName, OnComponentDrawFunc drawFunc );
    OnComponentDrawFunc Get( string_view componentName );

private:
    OnComponentDrawFuncStorage() = default;
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
        ImGui::Text( "Tag: " );
        ImGui::SameLine();
        ImGui::InputText( "##Tag", buffer, sizeof( buffer ) );
        tag.assign( buffer );
    }

    //void Serialize( const Loader& loader, nlohmann::json& out ) const;
    //void Deserialize( const nlohmann::json& j, Loader& loader );
};


struct TransformComponent : public mat4
{
    static string_view Name()
    {
        return "TransformComponent";
    }
    static void OnComponentDraw( void* raw )
    {
        auto& component = *(TransformComponent*)raw;

    }


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

    }

    //void Serialize( const Loader& loader, nlohmann::json& out ) const;
    //void Deserialize( const nlohmann::json& j, Loader& loader );
};

}