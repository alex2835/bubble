#include "engine/pch/pch.hpp"
#include "engine/scene/components/prefab_instance_component.hpp"
#include "engine/scene/components/component_draw_utils.hpp"
#include "engine/utils/imgui_utils.hpp"
#include <nlohmann/json.hpp>
#include <sol/sol.hpp>

namespace bubble
{
void PrefabInstanceComponent::OnComponentDraw( InspectorContext&, const Entity&, PrefabInstanceComponent& component )
{
    ImGui::TextColored( TEXT_COLOR, "PrefabInstanceComponent" );
    ImGui::Text( "prefab: %s", component.mPrefab.c_str() );
    ImGui::TextDisabled( "Edit the prefab, not this copy: the next update of its\n"
                         "instances replaces what is under this entity.\n"
                         "Remove this component to unpack the instance." );
}

void PrefabInstanceComponent::ToJson( json& json, const Project&, const PrefabInstanceComponent& component )
{
    json["Prefab"] = component.mPrefab;
}

void PrefabInstanceComponent::FromJson( const json& json, Project&, PrefabInstanceComponent& component )
{
    component.mPrefab = json.value( "Prefab", string() );
}

void PrefabInstanceComponent::CreateLuaBinding( sol::state& lua )
{
    lua.new_usertype<PrefabInstanceComponent>(
        "prefab_instance",
        "prefab", sol::readonly( &PrefabInstanceComponent::mPrefab )
    );
}

}
