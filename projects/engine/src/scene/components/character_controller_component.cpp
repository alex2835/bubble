#include "engine/pch/pch.hpp"
#include "engine/scene/components/character_controller_component.hpp"
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
CharacterControllerComponent::CharacterControllerComponent()
    : mController( CharacterController( 0.5f, 1.0f, 0.35f ) )
{
}

CharacterControllerComponent::CharacterControllerComponent( f32 radius, f32 height, f32 stepHeight )
    : mController( CharacterController( radius, height, stepHeight ) )
{
}

CharacterControllerComponent::CharacterControllerComponent( CharacterController controller )
    : mController( std::move( controller ) )
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
    lua.new_usertype<CharacterController>(
        "CharacterController",
        sol::constructors<CharacterController( f32, f32, f32 )>(),

        "SetWalkDirection",          &CharacterController::SetWalkDirection,
        "SetVelocityForTimeInterval",&CharacterController::SetVelocityForTimeInterval,
        "Jump",                      &CharacterController::Jump,
        "Warp",                      &CharacterController::Warp,
        "IsOnGround",                &CharacterController::IsOnGround,
        "GetPosition",               &CharacterController::GetPosition,
        "GetLinearVelocity",         &CharacterController::GetLinearVelocity,
        "SetMaxJumpHeight",          &CharacterController::SetMaxJumpHeight,
        "SetJumpSpeed",              &CharacterController::SetJumpSpeed,
        "SetFallSpeed",              &CharacterController::SetFallSpeed,
        "SetGravity",                &CharacterController::SetGravity,
        "SetMaxSlope",               &CharacterController::SetMaxSlope,
        "SetStepHeight",             &CharacterController::SetStepHeight,
        "GetRadius",                 &CharacterController::GetRadius,
        "GetHeight",                 &CharacterController::GetHeight
    );

    lua.new_usertype<CharacterControllerComponent>(
        "CharacterControllerComponent",
        "Controller", &CharacterControllerComponent::mController
    );

    lua["CreateCharacterController"] = []( f32 radius, f32 height, f32 stepHeight ) {
        return CharacterController( radius, height, stepHeight );
    };
}

}
