
#include "engine/scene/components.hpp"
#include "engine/utils/imgui_utils.hpp"
#include "engine/serialization/types_serialization.hpp"
#include <nlohmann/json.hpp>
#include <sol/sol.hpp>

namespace bubble
{
constexpr auto TEXT_COLOR = ImVec4( 1, 1, 0, 1 );

// TagComponent
void TagComponent::OnComponentDraw( const Loader& loader, TagComponent& tagComponent )
{
    ImGui::TextColored( TEXT_COLOR, "TagComponent" );
    ImGui::InputText( tagComponent.mName );
}

void TagComponent::ToJson( const Loader& loader, json& json, const TagComponent& tagComponent )
{
    json = tagComponent.mName;
}

void TagComponent::FromJson( Loader& loader, const json& json, TagComponent& tagComponent )
{
    tagComponent.mName = json;
}

void TagComponent::CreateLuaBinding( sol::state& lua )
{
    lua.new_usertype<TagComponent>(
        "Tag",
        sol::call_constructor,
        sol::constructors<TagComponent(), TagComponent(string)>(),
        "Name",
        &TagComponent::mName
    );
}

TagComponent::TagComponent( string name ) 
    : mName( std::move( name ) )
{}

// TransformComponent
void TransformComponent::OnComponentDraw( const Loader& loader, TransformComponent& transformComponent )
{
    ImGui::TextColored( TEXT_COLOR, "TransformComponent" );
    ImGui::DragFloat3( "Scale", (float*)&transformComponent.mScale, 0.01f, 0.01f );
    ImGui::DragFloat3( "Rotation", (float*)&transformComponent.mRotation, 0.01f );
    ImGui::DragFloat3( "Position", (float*)&transformComponent.mPosition, 0.1f );
}

TransformComponent::TransformComponent( vec3 pos, vec3 rot, vec3 scale )
    : mPosition( pos ),
      mRotation( rot ),
      mScale( scale )
{}

mat4 TransformComponent::TransformMat()
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
    auto to_string = []( const TransformComponent& t )
    {
        const vec3& p = t.mPosition;
        const vec3& r = t.mRotation;
        const vec3& s = t.mScale;
        return std::format( "pos: [{},{},{}], rot: [{},{},{}], scale: [{},{},{}]",
                            p.x, p.y, p.z, r.x, r.y, r.z, s.x, s.y, s.z );
    };

    lua.new_usertype<TransformComponent>(
        "Transform",
        sol::call_constructor,
        sol::constructors<TransformComponent(), TransformComponent(vec3, vec3, vec3)>(),
        "Position", 
        &TransformComponent::mPosition,
        "Rotation",
        &TransformComponent::mRotation,
        "Scale",
        &TransformComponent::mScale,
        sol::meta_function::to_string,
        to_string
    );
}

// LightComponent
void LightComponent::OnComponentDraw( const Loader& loader, LightComponent& lightComponent )
{
    ImGui::TextColored( TEXT_COLOR, "LightComponent" );
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
    ImGui::TextColored( TEXT_COLOR, "ModelComponent" );

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
    if ( not modelComponent )
    {
        json = nullptr;
        return;
    }
    auto [relPath, _] = loader.RelAbsFromProjectPath( modelComponent->mPath );
    json = relPath;
}

void ModelComponent::FromJson( Loader& loader, const json& json, ModelComponent& modelComponent )
{
    if ( not json.is_null() )
        modelComponent = loader.LoadModel( json );
}

void ModelComponent::CreateLuaBinding( sol::state& lua )
{
    lua.new_usertype<Model>(
        "Model",
        sol::meta_function::to_string,
        []( const Model& model ) { return model.mName; }
    );
}

// ShaderComponent
void ShaderComponent::OnComponentDraw( const Loader& loader, ShaderComponent& shaderComponent )
{
    ImGui::TextColored( TEXT_COLOR, "ShaderComponent" );

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
    if ( not shaderComponent )
    {
        json = nullptr;
        return;
    }

    auto [relPath, _] = loader.RelAbsFromProjectPath( shaderComponent->mPath );
    json = relPath;
}

void ShaderComponent::FromJson( Loader& loader, const json& json, ShaderComponent& shaderComponent )
{
    if ( not json.is_null() )
        shaderComponent = loader.LoadShader( json );
}

void ShaderComponent::CreateLuaBinding( sol::state& lua )
{
    lua.new_usertype<Shader>(
        "Shader",
        sol::meta_function::to_string,
        []( const Shader& shader ){ return shader.mName; }
    );
}

// ScriptComponent
void ScriptComponent::OnComponentDraw( const Loader& loader, ScriptComponent& scriptComponent )
{
    ImGui::TextColored( TEXT_COLOR, "ScriptComponent" );

    auto scriptComponentName = scriptComponent.mScript ? 
                               scriptComponent.mScript->mName.c_str() :
                               "Not selected";
    if ( ImGui::BeginCombo( "scripts", scriptComponentName ) )
    {
        for ( const auto& [scriptPath, shader] : loader.mScripts )
        {
            auto scriptName = scriptPath.stem().string();
            if ( ImGui::Selectable( scriptName.c_str(), scriptName == scriptComponentName ) )
                scriptComponent = shader;
        }
        ImGui::EndCombo();
    }
}

void ScriptComponent::ToJson( const Loader& loader, json& json, const ScriptComponent& scriptComponent )
{
    if ( not scriptComponent.mScript )
    {
        json = nullptr;
        return;
    }

    auto [relPath, _] = loader.RelAbsFromProjectPath( scriptComponent.mScript->mPath );
    json = relPath;
}

void ScriptComponent::FromJson( Loader& loader, const json& json, ScriptComponent& scriptComponent )
{
    if ( not json.is_null() )
        scriptComponent.mScript = loader.LoadScript( json );
}

void ScriptComponent::CreateLuaBinding( sol::state& lua )
{

}

ScriptComponent::ScriptComponent( const Ref<Script>& scirpt )
    : mScript( scirpt )
{

}

ScriptComponent::~ScriptComponent()
{

}

}