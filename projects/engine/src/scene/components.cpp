
#include "engine/scene/components.hpp"
#include "engine/project/project.hpp"
#include "engine/utils/imgui_utils.hpp"
#include "engine/serialization/types_serialization.hpp"
#include "engine/types/array.hpp"
#include "engine/types/string.hpp"
#include <nlohmann/json.hpp>
#include <sol/sol.hpp>

namespace bubble
{
constexpr auto TEXT_COLOR = ImVec4( 1, 1, 0, 1 );

// TagComponent
void TagComponent::OnComponentDraw( const Project& project, TagComponent& tagComponent )
{
    ImGui::TextColored( TEXT_COLOR, "TagComponent" );
    ImGui::InputText( tagComponent.mName );
}

void TagComponent::ToJson( json& json, const Project& project, const TagComponent& tagComponent )
{
    json = tagComponent.mName;
}

void TagComponent::FromJson( const json& json, Project& project, TagComponent& tagComponent )
{
    tagComponent.mName = json;
}

void TagComponent::CreateLuaBinding( sol::state& lua )
{
    lua.new_usertype<TagComponent>(
        "Tag",
        sol::call_constructor,
        sol::constructors<TagComponent(), TagComponent( string )>(),
        "Name",
        &TagComponent::mName
    );
}

TagComponent::TagComponent( string name )
    : mName( std::move( name ) )
{
}

// TransformComponent
void TransformComponent::OnComponentDraw( const Project& project, TransformComponent& transformComponent )
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
{
}

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

void TransformComponent::ToJson( json& json, const Project& project, const TransformComponent& transformComponent )
{
    json["Position"] = transformComponent.mPosition;
    json["Rotation"] = transformComponent.mRotation;
    json["Scale"] = transformComponent.mScale;
}

void TransformComponent::FromJson( const json& json, Project& project, TransformComponent& transformComponent )
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
        sol::constructors<TransformComponent(), TransformComponent( vec3, vec3, vec3 )>(),
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
void LightComponent::OnComponentDraw( const Project& project, LightComponent& lightComponent )
{
    ImGui::TextColored( TEXT_COLOR, "LightComponent" );
}

void LightComponent::ToJson( json& json, const Project& project, const LightComponent& raw )
{

}

void LightComponent::FromJson( const json& json, Project& project, LightComponent& lightComponent )
{

}

void LightComponent::CreateLuaBinding( sol::state& lua )
{
    lua.new_usertype<Light>(
        "Light"
    );
}

// ModelComponent
void ModelComponent::OnComponentDraw( const Project& project, ModelComponent& modelComponent )
{
    ImGui::TextColored( TEXT_COLOR, "ModelComponent" );

    auto modelComponentName = modelComponent ? modelComponent->mName.c_str() : "Not selected";
    if ( ImGui::BeginCombo( "models", modelComponentName ) )
    {
        for ( const auto& [modelPath, model] : project.mLoader.mModels )
        {
            auto modelName = modelPath.stem().string();
            if ( ImGui::Selectable( modelName.c_str(), modelName == modelComponentName ) )
                modelComponent = model;
        }
        ImGui::EndCombo();
    }
}

void ModelComponent::ToJson( json& json, const Project& project, const ModelComponent& modelComponent )
{
    if ( not modelComponent )
    {
        json = nullptr;
        return;
    }
    auto [relPath, _] = project.mLoader.RelAbsFromProjectPath( modelComponent->mPath );
    json = relPath;
}

void ModelComponent::FromJson( const json& json, Project& project, ModelComponent& modelComponent )
{
    if ( not json.is_null() )
        modelComponent = project.mLoader.LoadModel( json );
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
void ShaderComponent::OnComponentDraw( const Project& project, ShaderComponent& shaderComponent )
{
    ImGui::TextColored( TEXT_COLOR, "ShaderComponent" );

    auto shaderComponentName = shaderComponent ? shaderComponent->mName.c_str() : "Not selected";
    if ( ImGui::BeginCombo( "shaders", shaderComponentName ) )
    {
        for ( const auto& [shaderPath, shader] : project.mLoader.mShaders )
        {
            auto shaderName = shaderPath.stem().string();
            if ( ImGui::Selectable( shaderName.c_str(), shaderName == shaderComponentName ) )
                shaderComponent = shader;
        }
        ImGui::EndCombo();
    }
}

void ShaderComponent::ToJson( json& json, const Project& project, const ShaderComponent& shaderComponent )
{
    if ( not shaderComponent )
    {
        json = nullptr;
        return;
    }

    auto [relPath, _] = project.mLoader.RelAbsFromProjectPath( shaderComponent->mPath );
    json = relPath;
}

void ShaderComponent::FromJson( const json& json, Project& project, ShaderComponent& shaderComponent )
{
    if ( not json.is_null() )
        shaderComponent = project.mLoader.LoadShader( json );
}

void ShaderComponent::CreateLuaBinding( sol::state& lua )
{
    lua.new_usertype<Shader>(
        "Shader",
        sol::meta_function::to_string,
        []( const Shader& shader ) { return shader.mName; }
    );
}

// ScriptComponent
void ScriptComponent::OnComponentDraw( const Project& project, ScriptComponent& scriptComponent )
{
    ImGui::TextColored( TEXT_COLOR, "ScriptComponent" );

    auto scriptComponentName = scriptComponent.mScript ?
        scriptComponent.mScript->mName.c_str() :
        "Not selected";
    if ( ImGui::BeginCombo( "scripts", scriptComponentName ) )
    {
        for ( const auto& [scriptPath, shader] : project.mLoader.mScripts )
        {
            auto scriptName = scriptPath.stem().string();
            if ( ImGui::Selectable( scriptName.c_str(), scriptName == scriptComponentName ) )
                scriptComponent = shader;
        }
        ImGui::EndCombo();
    }
}

void ScriptComponent::ToJson( json& json, const Project& project, const ScriptComponent& scriptComponent )
{
    if ( not scriptComponent.mScript )
    {
        json = nullptr;
        return;
    }

    auto [relPath, _] = project.mLoader.RelAbsFromProjectPath( scriptComponent.mScript->mPath );
    json = relPath;
}

void ScriptComponent::FromJson( const json& json, Project& project, ScriptComponent& scriptComponent )
{
    if ( not json.is_null() )
        scriptComponent.mScript = project.mLoader.LoadScript( json );
}

void ScriptComponent::CreateLuaBinding( sol::state& lua )
{

}

ScriptComponent::ScriptComponent( const Ref<Script>& scirpt )
    : mScript( scirpt )
{

}

PhysicsComponent::PhysicsComponent()
    : mPhysicsObject( PhysicsObject::CreateSphere( vec3(), 1, 1 ) )
{

}

ScriptComponent::~ScriptComponent()
{

}



void PhysicsComponent::OnComponentDraw( const Project& project, PhysicsComponent& component )
{
    ImGui::TextColored( TEXT_COLOR, "Physics component" );

    const static map<int, string_view> shapes{ { SPHERE_SHAPE_PROXYTYPE, "sphere"sv } };
                                             //{ BOX_SHAPE_PROXYTYPE, "box"sv } };

    string_view curShapeName = "empty"sv;
    if ( component.mPhysicsObject )
    {
        auto iter = shapes.find( component.mPhysicsObject->getShape()->getShapeType() );
        if ( iter != shapes.end() )
            curShapeName = iter->second;
    }

    if ( ImGui::BeginCombo( "Collision shape", curShapeName.data() ) )
    {
        for ( const auto& [id, name] : shapes )
        {
            bool isSelected = curShapeName == name;
            if ( ImGui::Selectable( name.data(), isSelected ) )
            {
                if ( id == SPHERE_SHAPE_PROXYTYPE )
                    component.mPhysicsObject = PhysicsObject::CreateSphere( vec3(), 0, 1 );
            }
        }
        ImGui::EndCombo();
    }
    if ( not component.mPhysicsObject )
        return;

    auto body = component.mPhysicsObject->getBody();
    auto shape = component.mPhysicsObject->getShape();
    switch ( shape->getShapeType() )
    {
        case SPHERE_SHAPE_PROXYTYPE:
        {
            auto sphereShape = static_cast<btSphereShape*>( shape );

            float mass = body->getMass();
            float radius = sphereShape->getRadius();
            bool needRecreation = false;
            needRecreation |= ImGui::DragFloat( "Mass", &mass );
            needRecreation |= ImGui::DragFloat( "Radius", &radius );
            if ( needRecreation )
                component.mPhysicsObject = PhysicsObject::CreateSphere( vec3(), mass, radius );
        }
    }
}

void PhysicsComponent::ToJson( json& j, const Project& project, const PhysicsComponent& component )
{
    auto body = component.mPhysicsObject->getBody();
    auto shape = component.mPhysicsObject->getShape();
    switch ( shape->getShapeType() )
    {
        case SPHERE_SHAPE_PROXYTYPE:
        {
            auto sphereShape = static_cast<btSphereShape*>( shape );
            float mass = body->getMass();
            float radius = sphereShape->getRadius();
            j["Type"sv] = "Sphere"sv;
            j["Radius"sv] = radius;
            j["Mass"sv] = mass;
            return;
        }
    }
    throw std::runtime_error( "Invalid enum type" );
}

void PhysicsComponent::FromJson( const json& j, Project& project, PhysicsComponent& component )
{
    if ( j["Type"sv] == "Sphere"s )
    {
        component.mPhysicsObject = PhysicsObject::CreateSphere( vec3(), j["Mass"sv], j["Radius"sv] );
        return;
    }
    throw std::runtime_error( "Undefined physics component" );
}

void PhysicsComponent::CreateLuaBinding( sol::state& lua )
{

}

PhysicsComponent::PhysicsComponent( const Ref<PhysicsObject>& physicsObject )
    : mPhysicsObject( physicsObject )
{

}

PhysicsComponent::~PhysicsComponent()
{

}

}