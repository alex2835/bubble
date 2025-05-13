
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
    auto modelComp = TryGetComponent<ModelComponent>( project, entity );
    auto transComp = TryGetComponent<TransformComponent>( project, entity );
    if ( modelComp and transComp )
        return CalculateTransformedBBox( modelComp->get()->mBBox, transComp->ScaleMat() );
    return std::nullopt;
}



//============= Components =============

// TagComponent
void TagComponent::OnComponentDraw( const Project& project, const Entity& entity, TagComponent& tagComponent )
{
    ImGui::TextColored( TEXT_COLOR, "TagComponent" );
    ImGui::InputText( "##tag", tagComponent.mName );
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
void TransformComponent::OnComponentDraw( const Project& project, const Entity& entity, TransformComponent& transformComponent )
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

mat4 TransformComponent::TransformMat() const
{
    auto transform = mat4( 1.0f );
    transform = glm::translate( transform, mPosition );
    transform = glm::rotate( transform, mRotation.x, vec3( 1, 0, 0 ) );
    transform = glm::rotate( transform, mRotation.y, vec3( 0, 1, 0 ) );
    transform = glm::rotate( transform, mRotation.z, vec3( 0, 0, 1 ) );
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
    transform = glm::rotate( transform, mRotation.x, vec3( 1, 0, 0 ) );
    transform = glm::rotate( transform, mRotation.y, vec3( 0, 1, 0 ) );
    transform = glm::rotate( transform, mRotation.z, vec3( 0, 0, 1 ) );
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
void LightComponent::OnComponentDraw( const Project& project, const Entity& entity, LightComponent& lightComponent )
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
void ModelComponent::OnComponentDraw( const Project& project, const Entity& entity, ModelComponent& modelComponent )
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




// ShaderComponent
void ShaderComponent::OnComponentDraw( const Project& project, const Entity& entity, ShaderComponent& shaderComponent )
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
    : mPhysicsObject( PhysicsObject::CreateSphere( vec3(), 0, 1 ) )
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

    /// Physics shape selection
    string_view curShapeName = shapes[component.mPhysicsObject.getShape()->getShapeType()];
    if ( ImGui::BeginCombo( "Collision shape", curShapeName.data() ) )
    {
        for ( const auto& [id, name] : shapes )
        {
            bool isSelected = curShapeName == name;
            if ( ImGui::Selectable( name.data(), isSelected ) )
            {
                float mass = component.mPhysicsObject.getBody()->getMass();
                if ( id == SPHERE_SHAPE_PROXYTYPE )
                {
                    f32 radius = 1.0f;
                    if ( box )
                        radius = box->getShortestEdge() / 2;
                    component.mPhysicsObject = PhysicsObject::CreateSphere( vec3( 0 ), mass, radius );
                }
                if ( id == BOX_SHAPE_PROXYTYPE )
                {
                    vec3 halfExtend( 1 );
                    if ( box )
                        halfExtend = ( box->getMax() - box->getMin() ) * 0.5f;
                    component.mPhysicsObject = PhysicsObject::CreateBox( vec3( 0 ), mass, halfExtend );
                }
            }
        }
        ImGui::EndCombo();
    }

    /// Selected physics shape control
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
        } break;

        case BOX_SHAPE_PROXYTYPE:
        {
            auto boxShape = static_cast<btBoxShape*>( shape );

            float mass = body->getMass();
            btVector3 he = boxShape->getHalfExtentsWithMargin();
            auto halfExtends = vec3( he.x(), he.y(), he.z() );

            bool needRecreation = false;
            needRecreation |= ImGui::DragFloat( "Mass", &mass );
            needRecreation |= ImGui::DragFloat3( "Half Extends", &halfExtends.x );
            if ( needRecreation )
                component.mPhysicsObject = PhysicsObject::CreateBox( vec3(), mass, halfExtends );
        } break;
    }
}

void PhysicsComponent::ToJson( json& j, const Project& project, const PhysicsComponent& component )
{
    auto body = component.mPhysicsObject.getBody();
    auto shape = component.mPhysicsObject.getShape();
    switch ( shape->getShapeType() )
    {
        case SPHERE_SHAPE_PROXYTYPE:
        {
            auto sphereShape = static_cast<const btSphereShape*>( shape );
            float mass = body->getMass();
            float radius = sphereShape->getRadius();
            j["Type"sv] = "Sphere"sv;
            j["Radius"sv] = radius;
            j["Mass"sv] = mass;
            return;
        }

        case BOX_SHAPE_PROXYTYPE:
        {
            auto boxShape = static_cast<const btBoxShape*>( shape );
            float mass = body->getMass();
            btVector3 halfExtends = boxShape->getHalfExtentsWithMargin();
            j["Type"sv] = "Box"sv;
            j["HalfExtends"sv] = vec3( halfExtends.x(), halfExtends.y(), halfExtends.z() );
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
    if ( j["Type"sv] == "Box"s )
    {
        component.mPhysicsObject = PhysicsObject::CreateBox( vec3(), j["Mass"sv], j["HalfExtends"sv] );
        return;
    }
    throw std::runtime_error( "Undefined physics component" );
}

void PhysicsComponent::CreateLuaBinding( sol::state& lua )
{

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

StateComponent::StateComponent( const StateComponent& other )
{

}

StateComponent::~StateComponent()
{

}

void DrawFieldsAdding( Table table )
{
    auto isEmpty = table.empty();
    auto isArray = IsArray( table );
    int newId = (int)table.size() + 1;

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

    ImGui::SameLine();
    if ( ImGui::Button( "+", ImVec2( 20, 20 ) ) )
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

opt<Any> DrawAnyValue( Any any )
{
    if ( any.is<int>() )
    {
        auto value = any.as<int>();
        ImGui::SetNextItemWidth( 60.0f );
        ImGui::DragInt( "int", &value );
        return value;
    }
    else if ( any.is<float>() )
    {
        auto value = any.as<float>();
        ImGui::SetNextItemWidth( 60.0f );
        ImGui::DragFloat( "float", &value );
        return value;
    }
    else if ( any.is<std::string>() )
    {
        auto value = any.as<string>();
        ImGui::SetNextItemWidth( 100.0f );
        ImGui::InputText( "string", value );
        return value;
    }
    else if ( any.is<bool>() )
    {
        auto value = any.as<bool>();
        ImGui::Checkbox( "bool", &value );
        return value;
    }
    else if ( any.is<Table>() and IsArray( any.as<Table>() ) )
    {
        int i = 0;
        auto table = any.as<Table>();
        if ( ImGui::TreeNode( "array" ) )
        {
            for ( auto& [k, v] : table )
            {
                ImGui::PushID( ( i32 )reinterpret_cast<i64>( table.pointer() ) + i );

                auto name = std::to_string( k.as<int>() );
                ImGui::Text( name.c_str() );
                ImGui::SameLine( 100.0f );

                table[k] = DrawAnyValue( v );
                ImGui::SameLine();

                if ( ImGui::Button( "-" ) )
                    table[k] = sol::nil;

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
        if ( ImGui::TreeNode( "table" ) )
        {
            for ( auto& [k, v] : table )
            {
                ImGui::PushID( ( i32 )reinterpret_cast<i64>( table.pointer() ) + i );

                auto name = k.as<string>();
                ImGui::Text( name.c_str() );
                ImGui::SameLine( 100.0f );

                table[k] = DrawAnyValue( v );
                ImGui::SameLine();

                if ( ImGui::Button( "-" ) )
                    table[k] = sol::nil;

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
    DrawAnyValue( *component.mState );
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
        json j;
        auto table = v.as<Table>();
        for ( auto& [k, v] : table )
            j.push_back( SaveAnyValue( v ) );
        return std::move( j );
    }
    else if ( v.is<Table>() )
    {
        json j;
        auto table = v.as<Table>();
        for ( auto& [k, v] : table )
            j[k.as<string>()] = SaveAnyValue( v );
        return std::move( j );
    }
    else
        throw std::runtime_error( "Value of not supported type: " );

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
    throw std::runtime_error( "Value of not supported type: " );
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