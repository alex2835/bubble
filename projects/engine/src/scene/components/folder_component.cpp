#include "engine/pch/pch.hpp"
#include "engine/reflection/reflection.hpp"
#include "engine/scene/components/folder_component.hpp"
#include "engine/scene/components/component_draw_utils.hpp"
#include <nlohmann/json.hpp>
#include <sol/sol.hpp>

namespace bubble
{
void FolderComponent::Reflect()
{
    TypeBuilder<FolderComponent>( Name().data() )
        .Note( "Holds what is under it; moving it moves them." );
}
}
