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
#include "engine/reflection/reflection.hpp"
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

void TagComponent::Reflect()
{
    TypeBuilder<TagComponent>( Name().data() )
        .Field<&TagComponent::mName>( "name" )
        .Field<&TagComponent::mClass>( "class" );
}

void TagComponent::BindLuaMethods( sol::state&, sol::usertype<TagComponent>& type )
{
    type[sol::meta_function::to_string] = []( const TagComponent& tag )
    {
        return std::format( "name: {} class: {}", tag.mName, tag.mClass );
    };
}

TagComponent::TagComponent( string name, string cls )
    : mName( std::move( name ) ),
      mClass( std::move( cls ) )
{
}

}
