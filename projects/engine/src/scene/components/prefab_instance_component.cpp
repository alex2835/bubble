#include "engine/pch/pch.hpp"
#include "engine/reflection/reflection.hpp"
#include "engine/scene/components/prefab_instance_component.hpp"
#include "engine/scene/components/component_draw_utils.hpp"
#include "engine/utils/imgui_utils.hpp"
#include <nlohmann/json.hpp>
#include <sol/sol.hpp>

namespace bubble
{
void PrefabInstanceComponent::Reflect()
{
    TypeBuilder<PrefabInstanceComponent>( Name().data() )
        .Note( "Edit the prefab, not this copy: the next update of its\n"
               "instances replaces what is under this entity.\n"
               "Remove this component to unpack the instance." )
        // The .prefab file, relative to the project root.
        .Field<&PrefabInstanceComponent::mPrefab>( "prefab", { .mFlags = FieldInfo::ReadOnly } );
}
}
