#pragma once
#include <imgui.h>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/euler_angles.hpp>
#include "engine/utils/types.hpp"
#include "engine/utils/imexp.hpp"

namespace bubble
{
template <typename T>
concept CompnentType = requires( T comp, json& j, void* raw )
{
    { T::Name() } -> std::same_as<string_view>;
    { T::OnComponentDraw( raw ) } -> std::same_as<void>;
    { T::ToJson( j, raw ) } -> std::same_as<void>;
    { T::FromJson( j, raw ) } -> std::same_as<void>;
};

typedef void ( *OnComponentDrawFunc )( void* rawData );
typedef void ( *ComponentFromJson )( const json& j, void* rawData );
typedef void ( *ComponentToJson )( json& j, const void* rawData );
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

    template <CompnentType Component>
    static void Add()
    {
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
    strunomap<ComponentFunctions> mComponentFunctions;
};


}
