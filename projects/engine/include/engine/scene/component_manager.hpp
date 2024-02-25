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
    ComponentManager() = default;
    
    template <CompnentType Component>
    void Add()
    {
        AddOnDraw( string( Component::Name() ), Component::OnComponentDraw );
        AddToJson( string( Component::Name() ), Component::ToJson );
        AddFromJson( string( Component::Name() ), Component::FromJson );
    }

    void AddOnDraw( const string& componentName, OnComponentDrawFunc drawFunc );
    OnComponentDrawFunc GetOnDraw( string_view componentName );

    void AddFromJson( const string& componentName, ComponentFromJson drawFunc );
    ComponentFromJson GetFromJson( string_view componentName );

    void AddToJson( const string& componentName, ComponentToJson drawFunc );
    ComponentToJson GetToJson( string_view componentName );
private:
    strunomap<ComponentFunctions> mComponentFunctions;
};


}
