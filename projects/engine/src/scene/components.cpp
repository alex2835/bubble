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
    const auto* modelComponetPtr = TryGetComponent<ModelComponent>( project, entity );
    const auto* transComponetPtr = TryGetComponent<TransformComponent>( project, entity );
    if ( modelComponetPtr and modelComponetPtr->mModel and transComponetPtr )
        return CalculateTransformedBBox( modelComponetPtr->mModel->mBBox, transComponetPtr->ScaleMat() );
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

    lua.new_usertype<Transform>(
        "Transform",
        sol::call_constructor,
        sol::constructors<Transform(), Transform( vec3 ), Transform( vec3, vec3, vec3 )>(),
        "Position",
        &Transform::mPosition,
        "Rotation",
        &Transform::mRotation,
        "Scale",
        &Transform::mScale,
        sol::meta_function::to_string,
        to_string
    );
}


// CameraComponent
void CameraComponent::OnComponentDraw( const Project& project, const Entity& entity, CameraComponent& cameraComponent )
{
    ImGui::TextColored( TEXT_COLOR, "CameraComponent" );

    // Position
    //ImGui::DragFloat3( "Position", &cameraComponent.mPosition.x, 0.1f );

    // Direction vectors (read-only)
    //ImGui::Text( "Forward: (%.2f, %.2f, %.2f)",
    //             cameraComponent.mForward.x,
    //             cameraComponent.mForward.y,
    //             cameraComponent.mForward.z );
    //ImGui::Text( "Up: (%.2f, %.2f, %.2f)",
    //             cameraComponent.mUp.x,
    //             cameraComponent.mUp.y,
    //             cameraComponent.mUp.z );
    //ImGui::Text( "Right: (%.2f, %.2f, %.2f)",
    //             cameraComponent.mRight.x,
    //             cameraComponent.mRight.y,
    //             cameraComponent.mRight.z );

    // Euler angles
    //ImGui::DragFloat( "Yaw", &cameraComponent.mYaw, 0.01f );
    //ImGui::DragFloat( "Pitch", &cameraComponent.mPitch, 0.01f );

    // Clipping planes
    ImGui::DragFloat( "Near", &cameraComponent.mNear, 0.01f, 0.01f, cameraComponent.mFar );
    ImGui::DragFloat( "Far", &cameraComponent.mFar, 1.0f, cameraComponent.mNear, 10000.0f );

    // Field of view
    ImGui::SliderFloat( "FOV", &cameraComponent.mFov, 0.1f, 3.14f );

    // Speed settings
    //ImGui::DragFloat( "Max Speed", &cameraComponent.mMaxSpeed, 0.1f, 0.0f, 100.0f );
    //ImGui::DragFloat( "Mouse Sensitivity", &cameraComponent.mMouseSensitivity, 0.1f, 0.1f, 10.0f );

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
    if ( lightComponent.mType == LightType::Directional )
    {
        //if ( ImGui::DragFloat3( "Direction", &lightComponent.mDirection.x, 0.01f ) )
        //    lightComponent.mDirection = normalize( lightComponent.mDirection );
    }
    else if ( lightComponent.mType == LightType::Point )
    {
        //ImGui::DragFloat3( "Position", &lightComponent.mPosition.x, 0.1f );
        ImGui::SliderFloat( "Distance (meters)", &lightComponent.mDistance, 7.0f, 3250.0f, "%.1f m", ImGuiSliderFlags_Logarithmic );

        // Show calculated attenuation values (read-only)
        ImGui::Text( "Attenuation:" );
        ImGui::Indent();
        ImGui::Text( "Constant: %.3f", lightComponent.mConstant );
        ImGui::Text( "Linear: %.4f", lightComponent.mLinear );
        ImGui::Text( "Quadratic: %.6f", lightComponent.mQuadratic );
        ImGui::Unindent();
    }
    else if ( lightComponent.mType == LightType::Spot )
    {
        //ImGui::DragFloat3( "Position", &lightComponent.mPosition.x, 0.1f );
        //if ( ImGui::DragFloat3( "Direction", &lightComponent.mDirection.x, 0.01f ) )
        //    lightComponent.mDirection = normalize( lightComponent.mDirection );

        ImGui::SliderFloat( "Distance (meters)", &lightComponent.mDistance, 7.0f, 3250.0f, "%.1f m", ImGuiSliderFlags_Logarithmic );

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
            { "Directional", LightType::Directional },
            { "Point", LightType::Point },
            { "Spot", LightType::Spot }
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



// RigidBodyComponent
RigidBodyComponent::RigidBodyComponent()
    : mRigidBody( RigidBody::CreateSphere( 0, 1 ) )
{
}

RigidBodyComponent::RigidBodyComponent( RigidBody rigidBody )
    : mRigidBody( std::move( rigidBody ) )
{
}

RigidBodyComponent::~RigidBodyComponent()
{
}

void RigidBodyComponent::OnComponentDraw( const Project& project, const Entity& entity, RigidBodyComponent& component )
{
    ImGui::TextColored( TEXT_COLOR, "RigidBody component" );

    static map<int, string_view> shapes{ { SPHERE_SHAPE_PROXYTYPE, "sphere"sv },
                                         { BOX_SHAPE_PROXYTYPE, "box"sv },
                                         { CAPSULE_SHAPE_PROXYTYPE, "capsule"sv } };

    auto& rigidBody = component.mRigidBody;
    auto shape = rigidBody.getShape();
    auto box = TryGetEntityBBox( project, entity );

    f32 mass = rigidBody.GetMass();
    f32 friction = rigidBody.GetFriction();

    /// Physics shape selection combo
    string_view curShapeName = shapes[shape->getShapeType()];
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
                    component.mRigidBody = RigidBody::CreateSphere( mass, radius );
                }
                if ( id == BOX_SHAPE_PROXYTYPE )
                {
                    vec3 halfExtend( 1 );
                    if ( box )
                        halfExtend = ( box->getMax() - box->getMin() ) * 0.5f;
                    component.mRigidBody = RigidBody::CreateBox( mass, halfExtend );
                }
                if ( id == CAPSULE_SHAPE_PROXYTYPE )
                {
                    f32 radius = 0.5f;
                    f32 height = 1.0f;
                    if ( box )
                    {
                        vec3 size = box->getMax() - box->getMin();
                        radius = glm::min( size.x, size.z ) / 2.0f;
                        height = glm::max( 0.0f, size.y - 2.0f * radius );
                    }
                    component.mRigidBody = RigidBody::CreateCapsule( mass, radius, height );
                }
                component.mRigidBody.SetFriction( friction );
            }
        }
        ImGui::EndCombo();
        return;
    }

    /// Rigid body controls
    if ( ImGui::DragFloat( "Mass", &mass ) )
        rigidBody.SetMass( mass );

    if ( ImGui::DragFloat( "Friction", &friction ) )
        rigidBody.SetFriction( friction );

    /// Shape controls
    switch ( shape->getShapeType() )
    {
        case SPHERE_SHAPE_PROXYTYPE:
        {
            auto sphereShape = static_cast<btSphereShape*>( shape );
            f32 radius = sphereShape->getRadius();
            if ( ImGui::DragFloat( "Radius", &radius ) )
                component.mRigidBody = RigidBody::CreateSphere( mass, radius );
        } break;

        case BOX_SHAPE_PROXYTYPE:
        {
            auto boxShape = static_cast<btBoxShape*>( shape );
            btVector3 he = boxShape->getHalfExtentsWithMargin();
            auto halfExtends = vec3( he.x(), he.y(), he.z() );
            if ( ImGui::DragFloat3( "Half Extends", &halfExtends.x ) )
                component.mRigidBody = RigidBody::CreateBox( mass, halfExtends );
        } break;

        case CAPSULE_SHAPE_PROXYTYPE:
        {
            auto capsuleShape = static_cast<btCapsuleShape*>( shape );
            f32 radius = capsuleShape->getRadius();
            f32 height = capsuleShape->getHalfHeight() * 2.0f;
            bool changed = false;
            changed |= ImGui::DragFloat( "Radius", &radius, 0.01f, 0.01f, 100.0f );
            changed |= ImGui::DragFloat( "Height", &height, 0.01f, 0.0f, 100.0f );
            if ( changed )
                component.mRigidBody = RigidBody::CreateCapsule( mass, radius, height );
        } break;
    }
}

void RigidBodyComponent::ToJson( json& j, const Project& project, const RigidBodyComponent& component )
{
    const auto& rigidBody = component.mRigidBody;
    auto body = rigidBody.getBody();
    j["Mass"sv] = body->getMass();
    j["Friction"sv] = body->getFriction();

    auto shape = rigidBody.getShape();
    switch ( shape->getShapeType() )
    {
        case SPHERE_SHAPE_PROXYTYPE:
        {
            auto sphereShape = static_cast<const btSphereShape*>( shape );
            j["Shape"sv] = "Sphere"sv;
            j["Radius"sv] = sphereShape->getRadius();
            break;
        }
        case BOX_SHAPE_PROXYTYPE:
        {
            auto boxShape = static_cast<const btBoxShape*>( shape );
            btVector3 halfExtends = boxShape->getHalfExtentsWithMargin();
            j["Shape"sv] = "Box"sv;
            j["HalfExtends"sv] = vec3( halfExtends.x(), halfExtends.y(), halfExtends.z() );
            break;
        }
        case CAPSULE_SHAPE_PROXYTYPE:
        {
            auto capsuleShape = static_cast<const btCapsuleShape*>( shape );
            j["Shape"sv] = "Capsule"sv;
            j["Radius"sv] = capsuleShape->getRadius();
            j["Height"sv] = capsuleShape->getHalfHeight() * 2.0f;
            break;
        }
        default:
            throw std::runtime_error( "Invalid shape type" );
    }
}

void RigidBodyComponent::FromJson( const json& j, Project& project, RigidBodyComponent& component )
{
    f32 mass = j["Mass"sv];
    f32 friction = j["Friction"sv];

    string shape = j.value( "Shape"sv, j.value( "Type"sv, "Sphere"s ) ); // Support old format

    if ( shape == "Sphere"s )
        component.mRigidBody = RigidBody::CreateSphere( mass, j["Radius"sv] );
    else if ( shape == "Box"s )
        component.mRigidBody = RigidBody::CreateBox( mass, j["HalfExtends"sv] );
    else if ( shape == "Capsule"s )
        component.mRigidBody = RigidBody::CreateCapsule( mass, j["Radius"sv], j["Height"sv] );
    else
        throw std::runtime_error( "Undefined physics shape" );

    component.mRigidBody.SetFriction( friction );
}

void RigidBodyComponent::CreateLuaBinding( sol::state& lua )
{
    // RigidBody class bindings
    lua.new_usertype<RigidBody>(
        "RigidBody",

        // SetMass is set in physics_lua_bindings
        "GetMass",
        &RigidBody::GetMass,

        "SetFriction",
        &RigidBody::SetFriction,
        "GetFriction",
        &RigidBody::GetFriction,

        "ApplyCentralImpulse",
        &RigidBody::ApplyCentralImpulse,
        "ApplyTorqueImpulse",
        &RigidBody::ApplyTorqueImpulse
    );

    lua["CreateRigidBodySphere"] = []( const Transform& trans, f32 mass, f32 radius ){
        auto rigidBody = RigidBody::CreateSphere( mass, radius );
        rigidBody.SetTransform( trans.mPosition, trans.mRotation );
        return rigidBody;
    };

    lua["CreateRigidBodyBox"] = []( const Transform& trans, f32 mass, vec3 halfExtend ) {
        auto rigidBody = RigidBody::CreateBox( mass, halfExtend );
        rigidBody.SetTransform( trans.mPosition, trans.mRotation );
        return rigidBody;
    };

    lua["CreateRigidBodyCapsule"] = []( const Transform& trans, f32 mass, f32 radius, f32 height ) {
        auto rigidBody = RigidBody::CreateCapsule( mass, radius, height );
        rigidBody.SetTransform( trans.mPosition, trans.mRotation );
        return rigidBody;
    };

    //// RigidBodyComponent bindings
    //lua.new_usertype<RigidBodyComponent>(
    //    "RigidBodyComponent",
    //    "mRigidBody", &RigidBodyComponent::mRigidBody
    //);
}



// CharacterControllerComponent
CharacterControllerComponent::CharacterControllerComponent()
    : mController( CharacterController( 0.5f, 1.0f, 0.35f ) )
{
}

CharacterControllerComponent::CharacterControllerComponent( f32 radius, f32 height, f32 stepHeight )
    : mController( CharacterController( radius, height, stepHeight ) )
{
}

CharacterControllerComponent::~CharacterControllerComponent()
{
}

void CharacterControllerComponent::OnComponentDraw( const Project& project, const Entity& entity, CharacterControllerComponent& component )
{
    ImGui::TextColored( TEXT_COLOR, "CharacterController component" );

    auto& controller = component.mController;

    // Capsule shape controls
    f32 radius = controller.GetRadius();
    f32 height = controller.GetHeight();
    bool shapeChanged = false;
    shapeChanged |= ImGui::DragFloat( "Radius", &radius, 0.01f, 0.1f, 10.0f );
    shapeChanged |= ImGui::DragFloat( "Height", &height, 0.01f, 0.0f, 10.0f );
    if ( shapeChanged )
    {
        // Recreate with new dimensions, preserving other settings
        f32 stepHeight = controller.GetStepHeight();
        f32 jumpSpeed = controller.GetJumpSpeed();
        f32 fallSpeed = controller.GetFallSpeed();
        f32 maxSlope = controller.GetMaxSlopeRadians();
        vec3 gravity = controller.GetGravity();
        vec3 pos = controller.GetPosition();

        component.mController = CharacterController( radius, height, stepHeight );
        component.mController.SetJumpSpeed( jumpSpeed );
        component.mController.SetFallSpeed( fallSpeed );
        component.mController.SetMaxSlope( maxSlope );
        component.mController.SetGravity( gravity );
        component.mController.Warp( pos );
    }
    ImGui::Text( "Total Height: %.2f", height + 2.0f * radius );

    ImGui::Separator();

    // Movement configuration
    f32 stepHeight = controller.GetStepHeight();
    if ( ImGui::DragFloat( "Step Height", &stepHeight, 0.01f, 0.0f, 2.0f ) )
        controller.SetStepHeight( stepHeight );

    f32 maxSlopeDegrees = glm::degrees( controller.GetMaxSlopeRadians() );
    if ( ImGui::SliderFloat( "Max Slope (deg)", &maxSlopeDegrees, 0.0f, 90.0f ) )
        controller.SetMaxSlope( glm::radians( maxSlopeDegrees ) );

    ImGui::Separator();

    // Jump/Fall configuration
    f32 jumpSpeed = controller.GetJumpSpeed();
    if ( ImGui::DragFloat( "Jump Speed", &jumpSpeed, 0.1f, 0.0f, 50.0f ) )
        controller.SetJumpSpeed( jumpSpeed );

    f32 fallSpeed = controller.GetFallSpeed();
    if ( ImGui::DragFloat( "Fall Speed", &fallSpeed, 0.1f, 0.0f, 100.0f ) )
        controller.SetFallSpeed( fallSpeed );

    vec3 gravity = controller.GetGravity();
    if ( ImGui::DragFloat3( "Gravity", &gravity.x, 0.1f ) )
        controller.SetGravity( gravity );

    ImGui::Separator();

    // Runtime info (read-only)
    ImGui::Text( "Runtime Info:" );
    ImGui::Text( "On Ground: %s", controller.IsOnGround() ? "Yes" : "No" );
    vec3 pos = controller.GetPosition();
    ImGui::Text( "Position: (%.2f, %.2f, %.2f)", pos.x, pos.y, pos.z );
    vec3 vel = controller.GetLinearVelocity();
    ImGui::Text( "Velocity: (%.2f, %.2f, %.2f)", vel.x, vel.y, vel.z );
}

void CharacterControllerComponent::ToJson( json& j, const Project& project, const CharacterControllerComponent& component )
{
    const auto& controller = component.mController;
    j["Radius"sv] = controller.GetRadius();
    j["Height"sv] = controller.GetHeight();
    j["StepHeight"sv] = controller.GetStepHeight();
    j["JumpSpeed"sv] = controller.GetJumpSpeed();
    j["FallSpeed"sv] = controller.GetFallSpeed();
    j["MaxSlopeRadians"sv] = controller.GetMaxSlopeRadians();
    j["Gravity"sv] = controller.GetGravity();
}

void CharacterControllerComponent::FromJson( const json& j, Project& project, CharacterControllerComponent& component )
{
    f32 radius = j["Radius"sv];
    f32 height = j["Height"sv];
    f32 stepHeight = j.value( "StepHeight"sv, 0.35f );
    component.mController = CharacterController( radius, height, stepHeight );

    if ( j.contains( "JumpSpeed"sv ) )
        component.mController.SetJumpSpeed( j["JumpSpeed"sv] );
    if ( j.contains( "FallSpeed"sv ) )
        component.mController.SetFallSpeed( j["FallSpeed"sv] );
    if ( j.contains( "MaxSlopeRadians"sv ) )
        component.mController.SetMaxSlope( j["MaxSlopeRadians"sv] );
    if ( j.contains( "Gravity"sv ) )
        component.mController.SetGravity( j["Gravity"sv] );
}

void CharacterControllerComponent::CreateLuaBinding( sol::state& lua )
{
    // CharacterControllerComponent bindings
    //lua.new_usertype<CharacterControllerComponent>(
    //    "CharacterControllerComponent",
    //    "mController", []( CharacterControllerComponent& c ) -> CharacterController& { return c.mController; }
    //);


    // CharacterController bindings
    lua.new_usertype<CharacterController>(
        "CharacterController",
        sol::constructors<CharacterController( f32, f32, f32 )>(),

        "SetWalkDirection", &CharacterController::SetWalkDirection,
        "SetVelocityForTimeInterval", &CharacterController::SetVelocityForTimeInterval,
        "Jump", &CharacterController::Jump,
        "Warp", &CharacterController::Warp,

        "IsOnGround", &CharacterController::IsOnGround,
        "GetPosition", &CharacterController::GetPosition,
        "GetLinearVelocity", &CharacterController::GetLinearVelocity,

        "SetMaxJumpHeight", &CharacterController::SetMaxJumpHeight,
        "SetJumpSpeed", &CharacterController::SetJumpSpeed,
        "SetFallSpeed", &CharacterController::SetFallSpeed,
        "SetGravity", &CharacterController::SetGravity,
        "SetMaxSlope", &CharacterController::SetMaxSlope,
        "SetStepHeight", &CharacterController::SetStepHeight,

        "GetRadius", &CharacterController::GetRadius,
        "GetHeight", &CharacterController::GetHeight
    );

    lua["CreateCharacterController"] = []( f32 radius, f32 height, f32 stepHeight ) {
        return CharacterController( radius, height, stepHeight );
    };
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