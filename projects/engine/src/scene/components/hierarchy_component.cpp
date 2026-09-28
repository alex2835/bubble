#include "engine/pch/pch.hpp"
#include "engine/scene/components/hierarchy_component.hpp"
#include "engine/scene/components/component_draw_utils.hpp"
#include "engine/scene/components/tag_component.hpp"
#include "engine/editing/ui/inspector_context.hpp"
#include "engine/scene/scene.hpp"
#include "engine/scene/hierarchy.hpp"
#include "engine/utils/imgui_utils.hpp"
#include <nlohmann/json.hpp>
#include <sol/sol.hpp>

namespace bubble
{
// Read only: the links are made in the Entities tree - drag an entity onto
// another - and a field here that edited one side would break the other.
void HierarchyComponent::OnComponentDraw( InspectorContext& ctx, const Entity&, HierarchyComponent& component )
{
    ImGui::TextColored( TEXT_COLOR, "HierarchyComponent" );
    const auto name = [&]( Entity entity )
    {
        if ( not ctx.mScene.HasEntity( entity ) )
            return string( "-" );
        const string tag = NameOf( ctx.mScene, entity );
        return std::format( "{} ({})", tag, (u64)entity );
    };
    ImGui::Text( "parent: %s", name( component.mParent ).c_str() );
    ImGui::Text( "children: %zu", component.mChildren.size() );
    ImGui::TextDisabled( "Parent in the Entities tree: drag an entity onto another." );
}

// Both sides, the children in their order. The root has no "Parent": every
// id, 0 included, is an entity.
void HierarchyComponent::ToJson( json& json, const Project&, const HierarchyComponent& component )
{
    if ( component.mParent != Entity::Null )
        json["Parent"] = (u64)component.mParent;
    json["Children"] = json::array();
    for ( const Entity child : component.mChildren )
        json["Children"].push_back( (u64)child );
}

void HierarchyComponent::FromJson( const json& json, Project&, HierarchyComponent& component )
{
    const auto entity = []( u64 id ) { return Entity( static_cast<Entity::entity_type>( id ) ); };
    const auto parent = json.find( "Parent" );
    component.mParent = parent != json.end() ? entity( parent->get<u64>() ) : Entity::Null;
    component.mChildren.clear();
    if ( const auto children = json.find( "Children" ); children != json.end() and children->is_array() )
        for ( const auto& id : *children )
            component.mChildren.push_back( entity( id.get<u64>() ) );
}

void HierarchyComponent::CreateLuaBinding( sol::state& )
{
    // Scripts reach the hierarchy through Entity: get_parent, set_parent,
    // get_children (scene_lua_bindings.cpp).
}

}
