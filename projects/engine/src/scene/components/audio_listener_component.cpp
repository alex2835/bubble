#include "engine/pch/pch.hpp"
#include "engine/scene/components/audio_listener_component.hpp"
#include "engine/scene/components/component_draw_utils.hpp"
#include "engine/project/project.hpp"
#include "engine/utils/imgui_utils.hpp"
#include <nlohmann/json.hpp>
#include <sol/sol.hpp>

namespace bubble
{
void AudioListenerComponent::OnComponentDraw( EditContext& ctx, const Entity& entity, AudioListenerComponent& )
{
    ImGui::TextColored( TEXT_COLOR, "AudioListenerComponent" );
    CheckboxField<AudioListenerComponent>( ctx, entity, "Active", &AudioListenerComponent::mActive );
    ImGui::TextWrapped( "Position and orientation come from this entity's TransformComponent." );
}

void AudioListenerComponent::ToJson( json& json, const Project& project, const AudioListenerComponent& component )
{
    json["Active"] = component.mActive;
}

void AudioListenerComponent::FromJson( const json& json, Project& project, AudioListenerComponent& component )
{
    if ( not json.is_null() and json.contains( "Active" ) )
        component.mActive = json["Active"];
}

void AudioListenerComponent::CreateLuaBinding( sol::state& lua )
{
    lua.new_usertype<AudioListenerComponent>(
        "AudioListener",
        sol::call_constructor,
        sol::constructors<AudioListenerComponent()>(),
        "active", &AudioListenerComponent::mActive
    );
}

}
