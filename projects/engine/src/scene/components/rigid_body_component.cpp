#include "engine/pch/pch.hpp"
#include "engine/scene/components/rigid_body_component.hpp"
#include "engine/scene/components/component_draw_utils.hpp"
#include "engine/project/project.hpp"
#include "engine/utils/imgui_utils.hpp"
#include "engine/serialization/types_serialization.hpp"
#include "engine/types/array.hpp"
#include "engine/types/string.hpp"
#include "engine/utils/geometry.hpp"
#include <nlohmann/json.hpp>
#include <sol/sol.hpp>

namespace bubble
{
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
    lua.new_usertype<RigidBody>(
        "RigidBody",
        "get_mass",             &RigidBody::GetMass,
        "set_friction",         &RigidBody::SetFriction,
        "get_friction",         &RigidBody::GetFriction,
        "apply_central_impulse", &RigidBody::ApplyCentralImpulse,
        "apply_torque_impulse",  &RigidBody::ApplyTorqueImpulse
    );

    lua.new_usertype<RigidBodyComponent>(
        "RigidBodyComponent",
        "RigidBody", &RigidBodyComponent::mRigidBody
    );

    lua["create_rigid_body_sphere"] = []( const TransformComponent& trans, f32 mass, f32 radius ) {
        auto rigidBody = RigidBody::CreateSphere( mass, radius );
        rigidBody.SetTransform( trans.mPosition, trans.mRotation );
        return rigidBody;
    };
    lua["create_rigid_body_box"] = []( const TransformComponent& trans, f32 mass, vec3 halfExtend ) {
        auto rigidBody = RigidBody::CreateBox( mass, halfExtend );
        rigidBody.SetTransform( trans.mPosition, trans.mRotation );
        return rigidBody;
    };
    lua["create_rigid_body_capsule"] = []( const TransformComponent& trans, f32 mass, f32 radius, f32 height ) {
        auto rigidBody = RigidBody::CreateCapsule( mass, radius, height );
        rigidBody.SetTransform( trans.mPosition, trans.mRotation );
        return rigidBody;
    };
}

}
