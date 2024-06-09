#pragma once
#include <imgui.h>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/euler_angles.hpp>
#include "engine/utils/imexp.hpp"
#include "engine/utils/types.hpp"
#include "engine/loader/loader.hpp"
#include "engine/scene/scene.hpp"

namespace bubble
{
template <typename ComponentType>
concept ComponentConcept = requires( ComponentType component,
                                     Loader& loader,
                                     json& json,
                                     void* rawComponent )
{
    { ComponentType::Name() } -> std::same_as<string_view>;
    { ComponentType::OnComponentDraw( loader, rawComponent ) } -> std::same_as<void>;
    { ComponentType::ToJson( loader, json, rawComponent ) } -> std::same_as<void>;
    { ComponentType::FromJson( loader, json, rawComponent ) } -> std::same_as<void>;
};

typedef void ( *OnComponentDrawFunc )( const Loader& loader, void* rawData );
typedef void ( *ComponentToJson )( const Loader& loader, json& json, const void* rawData );
typedef void ( *ComponentFromJson )( Loader& loader, const json& json, void* rawData );

struct ComponentFunctions
{
    OnComponentDrawFunc mOnDraw;
    ComponentFromJson mFromJson;
    ComponentToJson mToJson;
};


class BUBBLE_ENGINE_EXPORT ComponentManager
{
public:
    static ComponentManager& Instance();

    static void SetScene( Scene& scene )
    {
        Instance().mScene = &scene;
    }

    template <ComponentConcept Component>
    static void Add()
    {
        auto scene = Instance().mScene;
        if ( not scene )
            throw std::runtime_error( "Scene is not set" );
        scene->AddComponet<Component>();

        AddOnDraw( Component::Name(), Component::OnComponentDraw );
        AddToJson( Component::Name(), Component::ToJson );
        AddFromJson( Component::Name(), Component::FromJson );
    }

    static void AddOnDraw( string_view componentName, OnComponentDrawFunc drawFunc );
    static OnComponentDrawFunc GetOnDraw( string_view componentName );

    static void AddFromJson( string_view componentName, ComponentFromJson drawFunc );
    static ComponentFromJson GetFromJson( string_view componentName );

    static void AddToJson( string_view componentName, ComponentToJson drawFunc );
    static ComponentToJson GetToJson( string_view componentName );
private:
    ComponentManager() = default;
    Scene* mScene = nullptr;
    str_hash_map<ComponentFunctions> mComponentFunctions;
};
}
