#pragma once
#include <string>
#include <string_view>
#include <algorithm>
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


struct TagComponent : public Component,
                      public string
{
    static string_view Name()
    {
        return "TagComponent";
    }

    TagComponent( string tag )
        : string( std::move( tag ) )
    {}

    //void Serialize( const Loader& loader, nlohmann::json& out ) const;
    //void Deserialize( const nlohmann::json& j, Loader& loader );
};


struct TransformComponent : public Component,
                            public mat4
{
    static string_view Name()
    {
        return "TransformComponent";
    }

    TransformComponent( const mat4& transform )
        : mat4( transform )
    {}

    //void Serialize( const Loader& loader, nlohmann::json& out ) const;
    //void Deserialize( const nlohmann::json& j, Loader& loader );
};

struct LightComponent : public Component,
                        public Light
{
    //void Serialize( const Loader& loader, nlohmann::json& out ) const;
    //void Deserialize( const nlohmann::json& j, Loader& loader );
};

struct ModelComponent : public Component, 
                        public Ref<Model>
{
    static string_view Name()
    {
        return "ModelComponent";
    }

    ModelComponent( const Ref<Model>& model )
        : Ref<Model>( model )
    {}
    

    //void Serialize( const Loader& loader, nlohmann::json& out ) const;
    //void Deserialize( const nlohmann::json& j, Loader& loader );
};

}