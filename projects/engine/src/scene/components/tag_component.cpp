#include "engine/pch/pch.hpp"
#include "engine/scene/components/tag_component.hpp"
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

}
