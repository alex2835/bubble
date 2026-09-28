#include "engine/pch/pch.hpp"
#include "engine/reflection/reflection.hpp"
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
// another - and a field that set one side would break the other. Both sides
// are saved, the children in their order; the root has no parent.
void HierarchyComponent::Reflect()
{
    TypeBuilder<HierarchyComponent>( Name().data() )
        .Note( "Parent in the Entities tree: drag an entity onto another." )
        .Field<&HierarchyComponent::mParent>( "parent", { .mFlags = FieldInfo::ReadOnly } )
        .Field<&HierarchyComponent::mChildren>( "children", { .mFlags = FieldInfo::ReadOnly } );
}

}
