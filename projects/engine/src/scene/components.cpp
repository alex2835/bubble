#include "engine/pch/pch.hpp"
#include "engine/scene/components.hpp"
#include "engine/project/project.hpp"
#include "engine/utils/imgui_utils.hpp"
#include "engine/serialization/types_serialization.hpp"
#include "engine/types/array.hpp"
#include "engine/types/string.hpp"
#include "engine/utils/geometry.hpp"
#include <nlohmann/json.hpp>
#include <sol/sol.hpp>

constexpr auto TEXT_COLOR = ImVec4( 1, 1, 0, 1 );
constexpr auto TABLE_FLAGS = ImGuiTreeNodeFlags_DefaultOpen |
                             ImGuiTreeNodeFlags_SpanAllColumns |
                             ImGuiTreeNodeFlags_Framed;

namespace bubble
{
//============= Helpers =============

template <typename T>
const T* TryGetComponent( const Project& project, Entity entity )
{
    if ( not project.mScene.HasComponent<T>( entity ) )
        return nullptr;
    return &project.mScene.GetComponent<T>( entity );
}

opt<AABB> TryGetEntityBBox( const Project& project, Entity entity )
{
    auto modelComponet = TryGetComponent<ModelComponent>( project, entity );
    auto transComponet = TryGetComponent<TransformComponent>( project, entity );
    if ( modelComponet->mModel and transComponet )
        return CalculateTransformedBBox( modelComponet->mModel->mBBox, transComponet->ScaleMat() );
    return std::nullopt;
}



//============= Components =============

// TagComponent
void TagComponent::OnComponentDraw( const Project& project, const Entity& entity, TagComponent& tagComponent )
{
    ImGui::TextColored( TEXT_COLOR, "TagComponent" );
    ImGui::InputText( "Name", tagComponent.mName );
    ImGui::InputText( "Class", tagComponent.mClass );
}

void TagComponent::ToJson( json& json, const Project& project, const TagComponent& tagComponent )
{
    json["Tag"] = tagComponent.mName;
    json["Class"] = tagComponent.mClass;
}

void TagComponent::FromJson( const json& json, Project& project, TagComponent& tagComponent )
{
    tagComponent.mName = json["Tag"];
    tagComponent.mClass = json["Class"];
}

void TagComponent::CreateLuaBinding( sol::state& lua )
{
    lua.new_usertype<TagComponent>(
        "Tag",
        sol::call_constructor,
        sol::constructors<TagComponent(), TagComponent( string ), TagComponent( string, string )>(),
        "Name",
        &TagComponent::mName,
        "Class",
        &TagComponent::mClass,
        sol::meta_function::to_string,
        []( const TagComponent& tag ) { return std::format( "Name: {} Class:{}", tag.mName, tag.mClass ); }
    );
}

TagComponent::TagComponent( string name, string cls )
    : mName( std::move( name ) ),
      mClass( std::move( cls ) )
{
}


// TransformComponent
void TransformComponent::OnComponentDraw( const Project& project, const Entity& entity, TransformComponent& transformComponent )
{
    ImGui::TextColored( TEXT_COLOR, "TransformComponent" );
    ImGui::DragFloat3( "Scale", (float*)&transformComponent.mScale, 0.01f, 0.01f );
    ImGui::DragFloat3( "Rotation", (float*)&transformComponent.mRotation, 0.01f );
    ImGui::DragFloat3( "Position", (float*)&transformComponent.mPosition, 0.1f );
}
 
mat4 TransformComponent::TransformMat() const
{
    auto transform = mat4( 1.0f );
    transform = glm::translate( transform, mPosition );
    transform = glm::rotate( transform, mRotation.z, vec3( 0, 0, 1 ) );
    transform = glm::rotate( transform, mRotation.y, vec3( 0, 1, 0 ) );
    transform = glm::rotate( transform, mRotation.x, vec3( 1, 0, 0 ) );
    transform = glm::scale( transform, mScale );
    return transform;
}

mat4 TransformComponent::ScaleMat() const
{
    auto transform = mat4( 1.0f );
    transform = glm::scale( transform, mScale );
    return transform;
}

mat4 TransformComponent::TranslationMat() const
{
    auto transform = mat4( 1.0f );
    transform = glm::translate( transform, mPosition );
    return transform;
}

mat4 TransformComponent::TranslationRotationMat() const
{
    auto transform = mat4( 1.0f );
    transform = glm::translate( transform, mPosition );
    transform = glm::rotate( transform, mRotation.z, vec3( 0, 0, 1 ) );
    transform = glm::rotate( transform, mRotation.y, vec3( 0, 1, 0 ) );
    transform = glm::rotate( transform, mRotation.x, vec3( 1, 0, 0 ) );
    return transform;
}


vec3 TransformComponent::Forward() const
{
    return normalize( vec3( 
        cos( mRotation.y ) * cos( mRotation.x ),
        sin( mRotation.x ),
        sin( mRotation.y ) * cos( mRotation.x )
    ) );
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


// CameraComponent
void CameraComponent::OnComponentDraw( const Project& project, const Entity& entity, CameraComponent& cameraComponent )
{
    ImGui::TextColored( TEXT_COLOR, "CameraComponent" );

    // Position
    ImGui::DragFloat3( "Position", &cameraComponent.mPosition.x, 0.1f );

    // Direction vectors (read-only)
    ImGui::Text( "Forward: (%.2f, %.2f, %.2f)",
                 cameraComponent.mForward.x,
                 cameraComponent.mForward.y,
                 cameraComponent.mForward.z );
    ImGui::Text( "Up: (%.2f, %.2f, %.2f)",
                 cameraComponent.mUp.x,
                 cameraComponent.mUp.y,
                 cameraComponent.mUp.z );
    ImGui::Text( "Right: (%.2f, %.2f, %.2f)",
                 cameraComponent.mRight.x,
                 cameraComponent.mRight.y,
                 cameraComponent.mRight.z );

    // Euler angles
    ImGui::DragFloat( "Yaw", &cameraComponent.mYaw, 0.01f );
    ImGui::DragFloat( "Pitch", &cameraComponent.mPitch, 0.01f );

    // Clipping planes
    ImGui::DragFloat( "Near", &cameraComponent.mNear, 0.01f, 0.01f, cameraComponent.mFar );
    ImGui::DragFloat( "Far", &cameraComponent.mFar, 1.0f, cameraComponent.mNear, 10000.0f );

    // Field of view
    ImGui::SliderFloat( "FOV", &cameraComponent.mFov, 0.1f, 3.14f );

    // Speed settings
    ImGui::DragFloat( "Max Speed", &cameraComponent.mMaxSpeed, 0.1f, 0.0f, 100.0f );
    ImGui::DragFloat( "Mouse Sensitivity", &cameraComponent.mMouseSensitivity, 0.1f, 0.1f, 10.0f );

    // Update camera vectors when angles change
    cameraComponent.EulerAnglesToVectors();
}

void CameraComponent::ToJson( json& json, const Project& project, const CameraComponent& cameraComponent )
{
    json["Position"] = cameraComponent.mPosition;
    json["Forward"] = cameraComponent.mForward;
    json["Up"] = cameraComponent.mUp;
    json["Right"] = cameraComponent.mRight;
    json["WorldUp"] = cameraComponent.mWorldUp;
    json["Near"] = cameraComponent.mNear;
    json["Far"] = cameraComponent.mFar;
    json["Fov"] = cameraComponent.mFov;
    json["Yaw"] = cameraComponent.mYaw;
    json["Pitch"] = cameraComponent.mPitch;
    json["MaxSpeed"] = cameraComponent.mMaxSpeed;
    json["MouseSensitivity"] = cameraComponent.mMouseSensitivity;
}

void CameraComponent::FromJson( const json& json, Project& project, CameraComponent& cameraComponent )
{
    if ( json.contains( "Position" ) )
        cameraComponent.mPosition = json["Position"];

    if ( json.contains( "Forward" ) )
        cameraComponent.mForward = json["Forward"];

    if ( json.contains( "Up" ) )
        cameraComponent.mUp = json["Up"];

    if ( json.contains( "Right" ) )
        cameraComponent.mRight = json["Right"];

    if ( json.contains( "WorldUp" ) )
        cameraComponent.mWorldUp = json["WorldUp"];

    if ( json.contains( "Near" ) )
        cameraComponent.mNear = json["Near"];

    if ( json.contains( "Far" ) )
        cameraComponent.mFar = json["Far"];

    if ( json.contains( "Fov" ) )
        cameraComponent.mFov = json["Fov"];

    if ( json.contains( "Yaw" ) )
        cameraComponent.mYaw = json["Yaw"];

    if ( json.contains( "Pitch" ) )
        cameraComponent.mPitch = json["Pitch"];

    if ( json.contains( "MaxSpeed" ) )
        cameraComponent.mMaxSpeed = json["MaxSpeed"];

    if ( json.contains( "MouseSensitivity" ) )
        cameraComponent.mMouseSensitivity = json["MouseSensitivity"];

    // Update camera vectors after loading
    cameraComponent.EulerAnglesToVectors();
}

void CameraComponent::CreateLuaBinding( sol::state& lua )
{
    lua.new_usertype<Camera>(
        "Camera",
        sol::call_constructor,
        sol::constructors<Camera(), Camera( vec3, f32, f32, f32, vec3 )>(),

        // Attributes
        "Position", &Camera::mPosition,
        "Forward", &Camera::mForward,
        "Up", &Camera::mUp,
        "Right", &Camera::mRight,
        "WorldUp", &Camera::mWorldUp,
        "Near", &Camera::mNear,
        "Far", &Camera::mFar,
        "Fov", &Camera::mFov,
        "Yaw", &Camera::mYaw,
        "Pitch", &Camera::mPitch,
        "MaxSpeed", &Camera::mMaxSpeed,
        "MouseSensitivity", &Camera::mMouseSensitivity,

        // Methods
        "GetLookatMat", &Camera::GetLookatMat,
        "GetProjectionMat", &Camera::GetProjectionMat,
        "ProcessMovement", &Camera::ProcessMovement,
        "ProcessMouseMovement", &Camera::ProcessMouseMovement,
        "ProcessMouseMovementOffset", &Camera::ProcessMouseMovementOffset,
        "ProcessMouseScroll", &Camera::ProcessMouseScroll,
        "EulerAnglesToVectors", &Camera::EulerAnglesToVectors,
        "OnUpdateFreeCamera", &Camera::OnUpdateFreeCamera,
        "ProcessRotation", &Camera::ProcessRotation,
        "OnUpdateThirdPerson", &Camera::OnUpdateThirdPerson
    );

    // CameraMovement enum
    lua.new_enum<CameraMovement>(
        "CameraMovement",
        {
            { "FORWARD", CameraMovement::FORWARD },
            { "BACKWARD", CameraMovement::BACKWARD },
            { "LEFT", CameraMovement::LEFT },
            { "RIGHT", CameraMovement::RIGHT },
            { "UP", CameraMovement::UP },
            { "DOWN", CameraMovement::DOWN }
        }
    );
}



// LightComponent
void LightComponent::OnComponentDraw( const Project& project, const Entity& entity, LightComponent& lightComponent )
{
    ImGui::TextColored( TEXT_COLOR, "LightComponent" );

    // Light Type Selection
    const char* lightTypes[] = { "Directional", "Point", "Spot" };
    int currentType = static_cast<int>( lightComponent.mType );
    if ( ImGui::Combo( "Type", &currentType, lightTypes, IM_ARRAYSIZE( lightTypes ) ) )
    {
        lightComponent.mType = static_cast<LightType>( currentType );
    }

    // Color
    ImGui::ColorEdit3( "Color", &lightComponent.mColor.x );

    // Brightness
    ImGui::DragFloat( "Brightness", &lightComponent.mBrightness, 0.01f, 0.0f, 10.0f );

    // Type-specific properties
    if ( lightComponent.mType == LightType::DirLight )
    {
        //if ( ImGui::DragFloat3( "Direction", &lightComponent.mDirection.x, 0.01f ) )
        //    lightComponent.mDirection = normalize( lightComponent.mDirection );
    }
    else if ( lightComponent.mType == LightType::PointLight )
    {
        //ImGui::DragFloat3( "Position", &lightComponent.mPosition.x, 0.1f );
        if ( ImGui::SliderFloat( "Distance", &lightComponent.mDistance, 0.0f, 1.0f ) )
            lightComponent.SetDistance( lightComponent.mDistance );

        // Show calculated attenuation values (read-only)
        ImGui::Text( "Attenuation:" );
        ImGui::Indent();
        ImGui::Text( "Constant: %.3f", lightComponent.mConstant );
        ImGui::Text( "Linear: %.4f", lightComponent.mLinear );
        ImGui::Text( "Quadratic: %.6f", lightComponent.mQuadratic );
        ImGui::Unindent();
    }
    else if ( lightComponent.mType == LightType::SpotLight )
    {
        //ImGui::DragFloat3( "Position", &lightComponent.mPosition.x, 0.1f );
        //if ( ImGui::DragFloat3( "Direction", &lightComponent.mDirection.x, 0.01f ) )
        //    lightComponent.mDirection = normalize( lightComponent.mDirection );

        if ( ImGui::SliderFloat( "Distance", &lightComponent.mDistance, 0.0f, 1.0f ) )
            lightComponent.SetDistance( lightComponent.mDistance );

        ImGui::SliderFloat( "Cut Off", &lightComponent.mCutOff, 0.0f, 90.0f );
        ImGui::SliderFloat( "Outer Cut Off", &lightComponent.mOuterCutOff, 0.0f, 90.0f );

        // Show calculated attenuation values (read-only)
        ImGui::Text( "Attenuation:" );
        ImGui::Indent();
        ImGui::Text( "Constant: %.3f", lightComponent.mConstant );
        ImGui::Text( "Linear: %.4f", lightComponent.mLinear );
        ImGui::Text( "Quadratic: %.6f", lightComponent.mQuadratic );
        ImGui::Unindent();
    }
}

void LightComponent::ToJson( json& json, const Project& project, const LightComponent& light )
{
    json["Type"] = static_cast<int>( light.mType );
    json["Color"] = { light.mColor.x, light.mColor.y, light.mColor.z };
    json["Brightness"] = light.mBrightness;
    json["Position"] = { light.mPosition.x, light.mPosition.y, light.mPosition.z };
    json["Direction"] = { light.mDirection.x, light.mDirection.y, light.mDirection.z };
    json["Distance"] = light.mDistance;
    json["CutOff"] = light.mCutOff;
    json["OuterCutOff"] = light.mOuterCutOff;
    json["Constant"] = light.mConstant;
    json["Linear"] = light.mLinear;
    json["Quadratic"] = light.mQuadratic;
}

void LightComponent::FromJson( const json& json, Project& project, LightComponent& lightComponent )
{
    if ( json.contains( "Type" ) )
        lightComponent.mType = static_cast<LightType>( json["Type"].get<int>() );

    if ( json.contains( "Color" ) )
    {
        auto color = json["Color"];
        lightComponent.mColor = vec3( color[0], color[1], color[2] );
    }

    if ( json.contains( "Brightness" ) )
        lightComponent.mBrightness = json["Brightness"];

    if ( json.contains( "Position" ) )
    {
        auto pos = json["Position"];
        lightComponent.mPosition = vec3( pos[0], pos[1], pos[2] );
    }

    if ( json.contains( "Direction" ) )
    {
        auto dir = json["Direction"];
        lightComponent.mDirection = vec3( dir[0], dir[1], dir[2] );
    }

    if ( json.contains( "Distance" ) )
        lightComponent.mDistance = json["Distance"];

    if ( json.contains( "CutOff" ) )
        lightComponent.mCutOff = json["CutOff"];

    if ( json.contains( "OuterCutOff" ) )
        lightComponent.mOuterCutOff = json["OuterCutOff"];

    if ( json.contains( "Constant" ) )
        lightComponent.mConstant = json["Constant"];

    if ( json.contains( "Linear" ) )
        lightComponent.mLinear = json["Linear"];

    if ( json.contains( "Quadratic" ) )
        lightComponent.mQuadratic = json["Quadratic"];
}

void LightComponent::CreateLuaBinding( sol::state& lua )
{
    // LightType enum
    lua.new_enum<LightType>(
        "LightType",
        {
            { "Directional", LightType::DirLight },
            { "Point", LightType::PointLight },
            { "Spot", LightType::SpotLight }
        }
    );

    // Light component
    lua.new_usertype<Light>(
        "Light",
        sol::call_constructor,
        sol::constructors<Light()>(),

        // Properties
        "Type", &Light::mType,
        "Color", &Light::mColor,
        "Brightness", &Light::mBrightness,
        "Position", &Light::mPosition,
        "Direction", &Light::mDirection,
        "Distance", &Light::mDistance,
        "CutOff", &Light::mCutOff,
        "OuterCutOff", &Light::mOuterCutOff,
        "Constant", &Light::mConstant,
        "Linear", &Light::mLinear,
        "Quadratic", &Light::mQuadratic,

        // Methods
        "SetDistance", &Light::SetDistance,

        // Static factory methods
        "CreateDirLight", &Light::CreateDirLight,
        "CreatePointLight", &Light::CreatePointLight,
        "CreateSpotLight", &Light::CreateSpotLight
    );
}




// ModelComponent
void ModelComponent::OnComponentDraw( const Project& project, const Entity& entity, ModelComponent& modelComponent )
{
    ImGui::TextColored( TEXT_COLOR, "ModelComponent" );

    const auto& model = modelComponent.mModel;
    auto modelName = model ? model->mName.c_str() : "Not selected";
    if ( ImGui::BeginCombo( "models", modelName ) )
    {
        for ( const auto& [modelPath, model] : project.mLoader.mModels )
        {
            auto modelComboName = modelPath.stem().string();
            if ( ImGui::Selectable( modelComboName.c_str(), modelComboName == modelName ) )
                modelComponent = model;
        }
        ImGui::EndCombo();
    }
}

void ModelComponent::ToJson( json& json, const Project& project, const ModelComponent& modelComponent )
{
    if ( not modelComponent.mModel )
    {
        json = nullptr;
        return;
    }
    auto [relPath, _] = project.mLoader.RelAbsFromProjectPath( modelComponent.mModel->mPath );
    json["Path"] = relPath;
}

void ModelComponent::FromJson( const json& json, Project& project, ModelComponent& modelComponent )
{
    if ( not json.is_null() )
        modelComponent = project.mLoader.LoadModel( json["Path"] );
}

void ModelComponent::CreateLuaBinding( sol::state& lua )
{
    lua.new_usertype<Model>(
        "Model",
        sol::meta_function::to_string,
        []( const Model& model ) { return model.mName; }
    );
}

ModelComponent::ModelComponent( const Ref<Model>& model )
    : mModel( model )
{
}

ModelComponent::~ModelComponent()
{

}


// ShaderComponent
void ShaderComponent::OnComponentDraw( const Project& project, const Entity& entity, ShaderComponent& shaderComponent )
{
    ImGui::TextColored( TEXT_COLOR, "ShaderComponent" );

    const auto& shader = shaderComponent.mShader;
    auto shaderName = shader ? shader->mName.c_str() : "Not selected";
    if ( ImGui::BeginCombo( "shaders", shaderName ) )
    {
        for ( const auto& [shaderPath, shader] : project.mLoader.mShaders )
        {
            auto shaderComboName = shaderPath.stem().string();
            if ( ImGui::Selectable( shaderComboName.c_str(), shaderComboName == shaderName ) )
                shaderComponent = shader;
        }
        ImGui::EndCombo();
    }
}

void ShaderComponent::ToJson( json& json, const Project& project, const ShaderComponent& shaderComponent )
{
    if ( not shaderComponent.mShader )
    {
        json = nullptr;
        return;
    }

    auto [relPath, _] = project.mLoader.RelAbsFromProjectPath( shaderComponent.mShader->mPath );
    json["Path"] = relPath;
}

void ShaderComponent::FromJson( const json& json, Project& project, ShaderComponent& shaderComponent )
{
    if ( not json.is_null() )
        shaderComponent = project.mLoader.LoadShader( json["Path"] );
}

void ShaderComponent::CreateLuaBinding( sol::state& lua )
{
    lua.new_usertype<Shader>(
        "Shader",
        sol::meta_function::to_string,
        []( const Shader& shader ) { return shader.mName; }
    );
}

ShaderComponent::ShaderComponent( const Ref<Shader>& shader )
    : mShader( shader )
{

}

ShaderComponent::~ShaderComponent()
{

}



// ScriptComponent
void ScriptComponent::OnComponentDraw( const Project& project, const Entity& entity, ScriptComponent& scriptComponent )
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

ScriptComponent::~ScriptComponent()
{

}



// PhysicsComponent
PhysicsComponent::PhysicsComponent()
    : mPhysicsObject( PhysicsObject::CreateSphere( 1 ) )
{

}

void PhysicsComponent::OnComponentDraw( const Project& project, const Entity& entity, PhysicsComponent& component )
{
    ImGui::TextColored( TEXT_COLOR, "Physics component" );

    static map<int, string_view> shapes{ { SPHERE_SHAPE_PROXYTYPE, "sphere"sv },
                                         { BOX_SHAPE_PROXYTYPE, "box"sv } };

    auto body = component.mPhysicsObject.getBody();
    auto shape = component.mPhysicsObject.getShape();
    auto box = TryGetEntityBBox( project, entity );

    /// Physics shape selection combo
    string_view curShapeName = shapes[component.mPhysicsObject.getShape()->getShapeType()];
    if ( ImGui::BeginCombo( "Collision shape", curShapeName.data() ) )
    {
        for ( const auto& [id, name] : shapes )
        {
            bool isSelected = curShapeName == name;
            if ( ImGui::Selectable( name.data(), isSelected ) )
            {
                if ( id == SPHERE_SHAPE_PROXYTYPE )
                {
                    f32 radius = 1.0f;
                    if ( box )
                        radius = box->getShortestEdge() / 2;
                    component.mPhysicsObject = PhysicsObject::CreateSphere( radius );
                }
                if ( id == BOX_SHAPE_PROXYTYPE )
                {
                    vec3 halfExtend( 1 );
                    if ( box )
                        halfExtend = ( box->getMax() - box->getMin() ) * 0.5f;
                    component.mPhysicsObject = PhysicsObject::CreateBox( halfExtend );
                }
                component.mPhysicsObject.SetMass( component.mPhysicsObject.getBody()->getMass() );
                component.mPhysicsObject.SetFriction( component.mPhysicsObject.getBody()->getFriction() );
            }
        }
        ImGui::EndCombo();
    }

    /// Shape controls
    switch ( shape->getShapeType() )
    {
        case SPHERE_SHAPE_PROXYTYPE:
        {
            auto sphereShape = static_cast<btSphereShape*>( shape );

            float radius = sphereShape->getRadius();
            if ( ImGui::DragFloat( "Radius", &radius ) )
                component.mPhysicsObject = PhysicsObject::CreateSphere( radius );
        } break;

        case BOX_SHAPE_PROXYTYPE:
        {
            auto boxShape = static_cast<btBoxShape*>( shape );

            btVector3 he = boxShape->getHalfExtentsWithMargin();
            auto halfExtends = vec3( he.x(), he.y(), he.z() );
            if ( ImGui::DragFloat3( "Half Extends", &halfExtends.x ) )
                component.mPhysicsObject = PhysicsObject::CreateBox( halfExtends );
        } break;
    }

    /// Rigid body controls
    float mass = component.mPhysicsObject.GetMass();
    if ( ImGui::DragFloat( "Mass", &mass ) )
        component.mPhysicsObject.SetMass( mass );

    float friction = component.mPhysicsObject.GetFriction();
    if ( ImGui::DragFloat( "Friction", &friction ) )
        component.mPhysicsObject.SetFriction( friction );
}

void PhysicsComponent::ToJson( json& j, const Project& project, const PhysicsComponent& component )
{
    auto body = component.mPhysicsObject.getBody();
    j["Mass"sv] = body->getMass();
    j["Friction"sv] = body->getFriction();

    auto shape = component.mPhysicsObject.getShape();
    switch ( shape->getShapeType() )
    {
        case SPHERE_SHAPE_PROXYTYPE:
        {
            auto sphereShape = static_cast<const btSphereShape*>( shape );
            float radius = sphereShape->getRadius();
            j["Type"sv] = "Sphere"sv;
            j["Radius"sv] = radius;
            return;
        }

        case BOX_SHAPE_PROXYTYPE:
        {
            auto boxShape = static_cast<const btBoxShape*>( shape );
            btVector3 halfExtends = boxShape->getHalfExtentsWithMargin();
            j["Type"sv] = "Box"sv;
            j["HalfExtends"sv] = vec3( halfExtends.x(), halfExtends.y(), halfExtends.z() );
            return;
        }
    }
    throw std::runtime_error( "Invalid enum type" );
}

void PhysicsComponent::FromJson( const json& j, Project& project, PhysicsComponent& component )
{
    if ( j["Type"sv] == "Sphere"s )
        component.mPhysicsObject = PhysicsObject::CreateSphere( j["Radius"sv] );
    else if ( j["Type"sv] == "Box"s )
        component.mPhysicsObject = PhysicsObject::CreateBox( j["HalfExtends"sv] );
    else
        throw std::runtime_error( "Undefined physics component" );

    component.mPhysicsObject.SetMass( j["Mass"sv] );
    component.mPhysicsObject.SetFriction( j["Friction"] );
}

void PhysicsComponent::CreateLuaBinding( sol::state& lua )
{
    lua.new_usertype<PhysicsObject>(
        "PhysicsComponent",

        // SetMass is set in lua binding
        "GetMass",
        &PhysicsObject::GetMass,

        "SetFriction",
        &PhysicsObject::SetFriction,
        "GetFriction",
        &PhysicsObject::GetFriction,

        "ApplyCentralImpulse",
        &PhysicsObject::ApplyCentralImpulse,
        "ApplyTorqueImpulse",
        &PhysicsObject::ApplyTorqueImpulse
    );

    lua["CreatePhysicsSphere"] = []( const TransformComponent& trans, f32 radius ){ 
        auto physicsSphere = PhysicsObject::CreateSphere( radius );
        physicsSphere.SetTransform( trans.mPosition, trans.mPosition );
        return physicsSphere;
    };

    lua["CreatePhysicsBox"] = []( const TransformComponent& trans, vec3 he ) {
        auto physicsBox = PhysicsObject::CreateBox( he );
        physicsBox.SetTransform( trans.mPosition, trans.mPosition );
        return physicsBox;
    };
}

PhysicsComponent::PhysicsComponent( const PhysicsObject& physicsObject )
    : mPhysicsObject( physicsObject )
{

}

PhysicsComponent::~PhysicsComponent()
{

}



// StateComponent
StateComponent::StateComponent()
{
    mState = CreateScope<Any>( Any{} );
}


Any AnyDeepCopy( Any any )
{
    if ( any.is<Table>() )
    {
        auto newAny = Any{};
        auto newTable = newAny.as<Table>();

        auto table = any.as<Table>();
        for ( auto& [k, v] : table )
            newTable[k] = v.is<Table>() ? AnyDeepCopy( v ) : v;

        return newAny;
    }
    return any;
}

StateComponent::StateComponent( const StateComponent& other )
{
    mState = CreateScope<Any>( AnyDeepCopy( *other.mState ) );
}

StateComponent& StateComponent::operator= ( const StateComponent& other )
{
    if ( this != &other )
        mState = CreateScope<Any>( AnyDeepCopy( *other.mState ) );
    return *this;
}

StateComponent::StateComponent( Any any )
{
    mState = CreateScope<Any>( AnyDeepCopy( any ) );
}

StateComponent::~StateComponent()
{

}

void DrawFieldsAdding( Table table )
{
    auto isEmpty = table.empty();
    auto isArray = IsArray( table );
    int newId = (int)table.size() + 1;

    bool addValue = ImGui::Button( "+", ImVec2( 20, 20 ) );
    ImGui::SameLine();

    static string fieldName;
    if ( not isArray )
    {
        ImGui::SetNextItemWidth( 100.0f );
        ImGui::InputText( "##state", fieldName );
        ImGui::SameLine();
    }
    if ( auto val = TryParseInt( fieldName ); 
         isEmpty and val and val >= 1 )
    {
        isArray = true;
        newId = *val;
    }

    ImGui::SetNextItemWidth( 100.0f );
    constexpr string_view types = "Int\0Float\0String\0Bool\0Table"sv;
    static enum class Types{ Int, Float, String, Bool, Table } selectedType;
    ImGui::Combo( "##type", (int*)&selectedType, types.data() );

    if ( addValue )
    {
        if ( fieldName.empty() )
            return;

        switch ( selectedType )
        {
            case Types::Int:
                if ( isArray )
                    table[newId] = 0;
                else
                    table[fieldName] = 0;
                break;
            case Types::Float:
                if ( isArray )
                    table[newId] = 0.0f;
                else
                    table[fieldName] = 0.0f;
                break;
            case Types::String:
                if ( isArray )
                    table[newId] = ""s;
                else
                    table[fieldName] = ""s;
                break;
            case Types::Bool:
                if ( isArray )
                    table[newId] = false;
                else
                    table[fieldName] = false;
                break;
            case Types::Table:
                table[fieldName] = Any{};
                break;
        }
    }
}

opt<Any> DrawAnyValue( const string& name, Any any )
{
    ImGui::SetNextItemWidth( 100.0f );
    if ( any.is<int>() )
    {
        auto value = any.as<int>();
        ImGui::DragInt( name.c_str(), &value );
        ImGui::SameLine();
        ImGui::Text( "(int)" );
        return value;
    }
    else if ( any.is<float>() )
    {
        auto value = any.as<float>();
        ImGui::DragFloat( name.c_str(), &value );
        ImGui::SameLine();
        ImGui::Text( "(float)" );
        return value;
    }
    else if ( any.is<std::string>() )
    {
        auto value = any.as<string>();
        ImGui::InputText( name.c_str(), value );
        ImGui::SameLine();
        ImGui::Text( "(string)" );
        return value;
    }
    else if ( any.is<bool>() )
    {
        auto value = any.as<bool>();
        ImGui::Checkbox( name.c_str(), &value );
        ImGui::SameLine();
        ImGui::Text( "(bool)" );
        return value;
    }
    else if ( any.is<Table>() and IsArray( any.as<Table>() ) )
    {
        int i = 0;
        auto table = any.as<Table>();
        if ( ImGui::TreeNodeEx( "##array", TABLE_FLAGS, "%s (array)", name.c_str() ) )
        {                                                                
            for ( auto& [k, v] : table )                                 
            {
                ImGui::PushID( ( i32 )reinterpret_cast<i64>( table.pointer() ) + i );

                if ( ImGui::Button( "-" ) )
                {
                    table[k] = sol::nil;
                    ImGui::PopID();
                    continue;
                }
                ImGui::SameLine();

                auto name = std::to_string( k.as<int>() );
                table[k] = DrawAnyValue( name, v );

                ImGui::Separator();
                ImGui::PopID();
                i++;
            }
            DrawFieldsAdding( table );
            ImGui::TreePop();
        }
        return table;
    }
    else if ( any.is<Table>() )
    {
        int i = 0;
        auto table = any.as<Table>();
        if ( ImGui::TreeNodeEx( "##table", TABLE_FLAGS, "%s (table)", name.c_str() ) )
        {
            for ( auto& [k, v] : table )
            {
                ImGui::PushID( ( i32 )reinterpret_cast<i64>( table.pointer() ) + i );

                if ( ImGui::Button( "-" ) )
                {
                    table[k] = sol::nil;
                    ImGui::PopID();
                    continue;
                }
                ImGui::SameLine();

                auto name = k.as<string>();
                table[k] = DrawAnyValue( name, v );

                ImGui::Separator();
                ImGui::PopID();
                i++;
            }
            DrawFieldsAdding( table );
            ImGui::TreePop();
        }
        return table;
    }
    throw std::runtime_error( "Invalid Any value type" );
}


void StateComponent::OnComponentDraw( const Project& project, const Entity& entity, StateComponent& component )
{
    ImGui::TextColored( TEXT_COLOR, "State component" );
    DrawAnyValue( "State"s, *component.mState );
}


json SaveAnyValue( Any v )
{
    if ( v.is<int>() )
        return v.as<int>();
    else if ( v.is<float>() )
        return v.as<float>();
    else if ( v.is<string>() )
        return v.as<string>();
    else if ( v.is<bool>() )
        return  v.as<bool>();
    else if ( v.is<Table>() and IsArray( v.as<Table>() ) )
    {
        json j = json::array();
        auto table = v.as<Table>();
        for ( auto& [k, v] : table )
            j.push_back( SaveAnyValue( v ) );
        return j;
    }
    else if ( v.is<Table>() )
    {
        json j = json::object();
        auto table = v.as<Table>();
        for ( auto& [k, v] : table )
            j[k.as<string>()] = SaveAnyValue( v );
        return j;
    }
    else
    {
        PrintAnyValue( v );
        throw std::runtime_error( "Value of not supported type" );
    }

}

void StateComponent::ToJson( json& json, const Project& project, const StateComponent& component )
{
    json = SaveAnyValue( *component.mState );
}


Any LoadAnyValue( const json& j )
{
    if ( j.is_number_integer() )
        return j.get<int>();
    else if ( j.is_number_float() )
        return j.get<float>();
    else if ( j.is_string() )
        return j.get<string>();
    else if ( j.is_boolean() )
        return j.get<bool>();
    else if ( j.is_array() )
    {
        auto val = Any{};
        auto table = val.as<Table>();
        int i = 1;
        for ( const auto& v : j )
           table[i++] = LoadAnyValue( v );
        return val;
    }
    else if ( j.is_object() )
    {
        auto val = Any{};
        auto table = val.as<Table>();
        for ( const auto& [k, v] : j.items() )
            table[k] = LoadAnyValue( v );
        return val;
    }
    throw std::runtime_error( std::format( "Value of not supported type: {}", string( j ) ) );
}

void StateComponent::FromJson( const json& json, Project& project, StateComponent& component )
{
    component.mState = CreateScope<Any>( LoadAnyValue( json ) );
}

void StateComponent::CreateLuaBinding( sol::state& lua )
{
    // It is native lua type
}


}