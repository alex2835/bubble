#include "engine/pch/pch.hpp"
#include "engine/scene/components/tag_component.hpp"
#include "engine/scene/components/component_draw_utils.hpp"
#include "engine/project/project.hpp"
#include "engine/editing/commands/tree_commands.hpp"
#include "engine/editing/history.hpp"
#include "engine/utils/imgui_utils.hpp"
#include "engine/serialization/types_serialization.hpp"
#include "engine/types/array.hpp"
#include "engine/types/string.hpp"
#include "engine/utils/geometry.hpp"
#include <nlohmann/json.hpp>
#include <sol/sol.hpp>

namespace bubble
{
void TagComponent::OnComponentDraw( InspectorContext& ctx, const Entity& entity, TagComponent& )
{
    ImGui::TextColored( TEXT_COLOR, "TagComponent" );

    // The name is a step in the tree: taken when the field is left, not per
    // key, and made unique among the siblings then - "chair" typed next to a
    // chair becomes chair2.
    static Entity sEditing = Entity::Null;
    static string sName;
    if ( sEditing != entity )
        sName = ctx.mScene.GetComponent<TagComponent>( entity ).mName;
    ImGui::InputText( "Name", sName );
    if ( ImGui::IsItemActivated() )
        sEditing = entity;
    if ( ImGui::IsItemDeactivated() and sEditing == entity )
    {
        sEditing = Entity::Null;
        if ( auto step = MakeRenameCommand( ctx.mScene, entity, sName ) )
            ctx.mHistory.Execute( std::move( step ) );
    }

    InputTextField<TagComponent>( ctx, entity, "Class", &TagComponent::mClass );
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
        "name",
        &TagComponent::mName,
        "class",
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
