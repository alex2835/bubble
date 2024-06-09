
#include "engine/scene/components.hpp"
#include "engine/utils/imgui_utils.hpp"
#include "engine/serialization/types_serialization.hpp"
#include <nlohmann/json.hpp>

namespace bubble
{
constexpr auto COLOR_YELLOW = ImVec4( 1, 1, 0, 1 );

// TagComponent
void TagComponent::OnComponentDraw( const Loader& loader, void* raw )
{
    auto& tag = *(TagComponent*)raw;
    ImGui::TextColored( COLOR_YELLOW, "TagComponent" );
    ImGui::InputText( tag );
}

void TagComponent::ToJson( const Loader& loader, json& json, const void* raw )
{
    json = *(const TagComponent*)raw;
}

void TagComponent::FromJson( Loader& loader, const json& json, void* raw )
{
    *(TagComponent*)raw = json;
}

// TransformComponent
void TransformComponent::OnComponentDraw( const Loader& loader, void* raw )
{
    auto& component = *(TransformComponent*)raw;
    ImGui::TextColored( COLOR_YELLOW, "TransformComponent" );
    ImGui::DragFloat3( "Scale", (float*)&component.mScale, 0.01f, 0.01f );
    ImGui::DragFloat3( "Rotation", (float*)&component.mRotation, 0.01f );
    ImGui::DragFloat3( "Position", (float*)&component.mPosition, 0.1f );
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

void TransformComponent::ToJson( const Loader& loader, json& json, const void* raw )
{
    const auto& transform = *(const TransformComponent*)raw;
    json["Position"] = transform.mPosition;
    json["Rotation"] = transform.mRotation;
    json["Scale"] = transform.mScale;
}

void TransformComponent::FromJson( Loader& loader, const json& json, void* raw )
{
    auto& transform = *(TransformComponent*)raw;
    transform.mPosition = json["Position"];
    transform.mRotation = json["Rotation"];
    transform.mScale = json["Scale"];
}

// LightComponent
void LightComponent::OnComponentDraw( const Loader& loader, void* raw )
{
    auto& component = *(LightComponent*)raw;
    ImGui::TextColored( COLOR_YELLOW, "LightComponent" );
}

void LightComponent::ToJson( const Loader& loader, json& json, const void* raw )
{
    const auto& light = *(const LightComponent*)raw;
}

void LightComponent::FromJson( Loader& loader, const json& json, void* raw )
{
    auto& light = *(LightComponent*)raw;
}

// ModelComponent
void ModelComponent::OnComponentDraw( const Loader& loader, void* raw )
{
    auto& modelComponent = *(ModelComponent*)raw;
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

void ModelComponent::ToJson( const Loader& loader, json& json, const void* raw )
{
    const auto& model = *(const ModelComponent*)raw;
    json = model->mPath;
}

void ModelComponent::FromJson( Loader& loader, const json& json, void* raw )
{
    auto& model = *(ModelComponent*)raw;
    model = loader.LoadModel( json );
}



// ShaderComponent
void ShaderComponent::OnComponentDraw( const Loader& loader, void* raw )
{
    auto& shaderComponent = *(ShaderComponent*)raw;
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

void ShaderComponent::ToJson( const Loader& loader, json& json, const void* raw )
{
    const auto& shader = *(const ShaderComponent*)raw;
    json = shader->mPath;
}

void ShaderComponent::FromJson( Loader& loader, const json& json, void* raw )
{
    auto& shader  = *(ShaderComponent*)raw;
    shader = loader.LoadShader( json );
}


}