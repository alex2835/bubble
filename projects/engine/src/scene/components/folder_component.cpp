#include "engine/pch/pch.hpp"
#include "engine/scene/components/folder_component.hpp"
#include "engine/scene/components/component_draw_utils.hpp"
#include <nlohmann/json.hpp>
#include <sol/sol.hpp>

namespace bubble
{
void FolderComponent::OnComponentDraw( InspectorContext&, const Entity&, FolderComponent& )
{
    ImGui::TextColored( TEXT_COLOR, "FolderComponent" );
    ImGui::TextDisabled( "Holds what is under it; moving it moves them." );
}

void FolderComponent::ToJson( json& json, const Project&, const FolderComponent& )
{
    json = json::object();
}

void FolderComponent::FromJson( const json&, Project&, FolderComponent& )
{
}

void FolderComponent::CreateLuaBinding( sol::state& )
{
}

}
