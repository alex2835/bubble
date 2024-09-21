
#include "engine/scene/components.hpp"
#include "engine/utils/imgui_utils.hpp"
#include "engine/serialization/types_serialization.hpp"
#include <nlohmann/json.hpp>
#include <sol/sol.hpp>

namespace bubble
{
constexpr auto COLOR_YELLOW = ImVec4( 1, 1, 0, 1 );

// TagComponent
void TagComponent::OnComponentDraw( const Loader& loader, TagComponent& tagComponent )
{
    ImGui::TextColored( COLOR_YELLOW, "TagComponent" );
    ImGui::InputText( tagComponent );
}

void TagComponent::ToJson( const Loader& loader, json& json, const TagComponent& tagComponent )
{
    json = tagComponent;
}

void TagComponent::FromJson( Loader& loader, const json& json, TagComponent& tagComponent )
{
    tagComponent = json;
}

void TagComponent::CreateLuaBinding( sol::state& lua )
{
    //lua.new_usertype<TagComponent>(
    //    "TagComponent",
    //);
}

// TransformComponent
void TransformComponent::OnComponentDraw( const Loader& loader, TransformComponent& transformComponent )
{
    ImGui::TextColored( COLOR_YELLOW, "TransformComponent" );
    ImGui::DragFloat3( "Scale", (float*)&transformComponent.mScale, 0.01f, 0.01f );
    ImGui::DragFloat3( "Rotation", (float*)&transformComponent.mRotation, 0.01f );
    ImGui::DragFloat3( "Position", (float*)&transformComponent.mPosition, 0.1f );
}

mat4 TransformComponent::Transform()
{
    auto transform = mat4( 1.0f );
    transform = glm::translate( transform, mPosition );
    transform = glm::rotate( transform, glm::radians( mRotation.x ), vec3( 1, 0, 0 ) );
    transform = glm::rotate( transform, glm::radians( mRotation.y ), vec3( 0, 1, 0 ) );
    transform = glm::rotate( transform, glm::radians( mRotation.z ), vec3( 0, 0, 1 ) );
    transform = glm::scale( transform, mScale );
    return transform;
}

void TransformComponent::ToJson( const Loader& loader, json& json, const TransformComponent& transformComponent )
{
    json["Position"] = transformComponent.mPosition;
    json["Rotation"] = transformComponent.mRotation;
    json["Scale"] = transformComponent.mScale;
}

void TransformComponent::FromJson( Loader& loader, const json& json, TransformComponent& transformComponent )
{
    transformComponent.mPosition = json["Position"];
    transformComponent.mRotation = json["Rotation"];
    transformComponent.mScale = json["Scale"];
}

void TransformComponent::CreateLuaBinding( sol::state& lua )
{
    lua.new_usertype<TransformComponent>(
        "TransformComponent",
        "Position",
        &TransformComponent::mPosition,
        "Rotation",
        &TransformComponent::mRotation,
        "Scale",
        &TransformComponent::mScale
    );
}

// LightComponent
void LightComponent::OnComponentDraw( const Loader& loader, LightComponent& lightComponent )
{
    ImGui::TextColored( COLOR_YELLOW, "LightComponent" );
}

void LightComponent::ToJson( const Loader& loader, json& json, const LightComponent& raw )
{

}

void LightComponent::FromJson( Loader& loader, const json& json, LightComponent& lightComponent )
{

}

void LightComponent::CreateLuaBinding( sol::state& lua )
{
    lua.new_usertype<Light>(
        "Light"
    );
}

// ModelComponent
void ModelComponent::OnComponentDraw( const Loader& loader, ModelComponent& modelComponent )
{
    ImGui::TextColored( COLOR_YELLOW, "ModelComponent" );

    auto modelComponentName = modelComponent ? modelComponent->mName.c_str() : "Not selected";
    if ( ImGui::BeginCombo( "models", modelComponentName ) )
    {
        for ( const auto& [modelPath, model] : loader.mModels )
        {
            auto modelName = modelPath.stem().string();
            if ( ImGui::Selectable( modelName.c_str(), modelName == modelComponentName ) )
                modelComponent = model;
        }
        ImGui::EndCombo();
    }
}

void ModelComponent::ToJson( const Loader& loader, json& json, const ModelComponent& modelComponent )
{
    auto [relPath, _] = loader.RelAbsFromProjectPath( modelComponent->mPath );
    json = relPath;
}

void ModelComponent::FromJson( Loader& loader, const json& json, ModelComponent& modelComponent )
{
    modelComponent = loader.LoadModel( json );
}

void ModelComponent::CreateLuaBinding( sol::state& lua )
{
    lua.new_usertype<Model>(
        "Model"
    );
}

// ShaderComponent
void ShaderComponent::OnComponentDraw( const Loader& loader, ShaderComponent& shaderComponent )
{
    ImGui::TextColored( COLOR_YELLOW, "ShaderComponent" );

    auto shaderComponentName = shaderComponent ? shaderComponent->mName.c_str() : "Not selected";
    if ( ImGui::BeginCombo( "shaders", shaderComponentName ) )
    {
        for ( const auto& [shaderPath, shader] : loader.mShaders )
        {
            auto shaderName = shaderPath.stem().string();
            if ( ImGui::Selectable( shaderName.c_str(), shaderName == shaderComponentName ) )
                shaderComponent = shader;
        }
        ImGui::EndCombo();
    }
}

void ShaderComponent::ToJson( const Loader& loader, json& json, const ShaderComponent& shaderComponent )
{
    auto [relPath, _] = loader.RelAbsFromProjectPath( shaderComponent->mPath );
    json = relPath;
}

void ShaderComponent::FromJson( Loader& loader, const json& json, ShaderComponent& shaderComponent )
{
    shaderComponent = loader.LoadShader( json );
}

void ShaderComponent::CreateLuaBinding( sol::state& lua )
{
    lua.new_usertype<Shader>(
        "Shader"
    );
}

}